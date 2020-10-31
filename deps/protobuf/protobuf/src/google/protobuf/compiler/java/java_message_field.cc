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
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_message_field.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {

void SetMessageVariables(const FieldDescriptor* descriptor, int messageBitIndex,
                         int builderBitIndex, const FieldGeneratorInfo* info,
                         ClassNameResolver* name_resolver,
                         std::map<std::string, std::string>* variables) {
  SetCommonFieldVariables(descriptor, info, variables);

  (*variables)["type"] =
      name_resolver->GetImmutableClassName(descriptor->message_type());
  (*variables)["mutable_type"] =
      name_resolver->GetMutableClassName(descriptor->message_type());
  (*variables)["group_or_message"] =
      (GetType(descriptor) == FieldDescriptor::TYPE_GROUP) ? "Group"
                                                           : "Message";
  // TODO(birdo): Add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] =
      descriptor->options().deprecated() ? "@java.lang.Deprecated " : "";
  (*variables)["on_changed"] = "onChanged();";
  (*variables)["ver"] = GeneratedCodeVersionSuffix();
  (*variables)["get_parser"] =
      ExposePublicParser(descriptor->message_type()->file()) ? "PARSER"
                                                             : "parser()";

  if (SupportFieldPresence(descriptor->file())) {
    // For singular messages and builders, one bit is used for the hasField bit.
    (*variables)["get_has_field_bit_message"] = GenerateGetBit(messageBitIndex);
    (*variables)["get_has_field_bit_builder"] = GenerateGetBit(builderBitIndex);

    // Note that these have a trailing ";".
    (*variables)["set_has_field_bit_message"] =
        GenerateSetBit(messageBitIndex) + ";";
    (*variables)["set_has_field_bit_builder"] =
        GenerateSetBit(builderBitIndex) + ";";
    (*variables)["clear_has_field_bit_builder"] =
        GenerateClearBit(builderBitIndex) + ";";

    (*variables)["is_field_present_message"] = GenerateGetBit(messageBitIndex);
  } else {
    (*variables)["set_has_field_bit_message"] = "";
    (*variables)["set_has_field_bit_builder"] = "";
    (*variables)["clear_has_field_bit_builder"] = "";

    (*variables)["is_field_present_message"] =
        (*variables)["name"] + "_ != null";
  }

  // For repated builders, one bit is used for whether the array is immutable.
  (*variables)["get_mutable_bit_builder"] = GenerateGetBit(builderBitIndex);
  (*variables)["set_mutable_bit_builder"] = GenerateSetBit(builderBitIndex);
  (*variables)["clear_mutable_bit_builder"] = GenerateClearBit(builderBitIndex);

  // For repeated fields, one bit is used for whether the array is immutable
  // in the parsing constructor.
  (*variables)["get_mutable_bit_parser"] =
      GenerateGetBitMutableLocal(builderBitIndex);
  (*variables)["set_mutable_bit_parser"] =
      GenerateSetBitMutableLocal(builderBitIndex);

  (*variables)["get_has_field_bit_from_local"] =
      GenerateGetBitFromLocal(builderBitIndex);
  (*variables)["set_has_field_bit_to_local"] =
      GenerateSetBitToLocal(messageBitIndex);
}

}  // namespace

// ===================================================================

ImmutableMessageFieldGenerator::ImmutableMessageFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  SetMessageVariables(descriptor, messageBitIndex, builderBitIndex,
                      context->GetFieldGeneratorInfo(descriptor),
                      name_resolver_, &variables_);
}

ImmutableMessageFieldGenerator::~ImmutableMessageFieldGenerator() {}

int ImmutableMessageFieldGenerator::GetNumBitsForMessage() const {
  return SupportFieldPresence(descriptor_->file()) ? 1 : 0;
}

int ImmutableMessageFieldGenerator::GetNumBitsForBuilder() const {
  return GetNumBitsForMessage();
}

void ImmutableMessageFieldGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  // TODO(jonp): In the future, consider having a method specific to the
  // interface so that builders can choose dynamically to either return a
  // message or a nested builder, so that asking for the interface doesn't
  // cause a message to ever be built.
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_, "$deprecation$boolean has$capitalized_name$();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_, "$deprecation$$type$ get$capitalized_name$();\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$$type$OrBuilder get$capitalized_name$OrBuilder();\n");
}

void ImmutableMessageFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  printer->Print(variables_, "private $type$ $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);

  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $get_has_field_bit_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
        "  return $name$_ == null ? $type$.getDefaultInstance() : $name$_;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);

    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public $type$OrBuilder "
        "${$get$capitalized_name$OrBuilder$}$() {\n"
        "  return $name$_ == null ? $type$.getDefaultInstance() : $name$_;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  } else {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $name$_ != null;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
        "  return $name$_ == null ? $type$.getDefaultInstance() : $name$_;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);

    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$public $type$OrBuilder "
                   "${$get$capitalized_name$OrBuilder$}$() {\n"
                   "  return get$capitalized_name$();\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
  }
}

void ImmutableMessageFieldGenerator::PrintNestedBuilderCondition(
    io::Printer* printer, const char* regular_case,
    const char* nested_builder_case) const {
  printer->Print(variables_, "if ($name$Builder_ == null) {\n");
  printer->Indent();
  printer->Print(variables_, regular_case);
  printer->Outdent();
  printer->Print("} else {\n");
  printer->Indent();
  printer->Print(variables_, nested_builder_case);
  printer->Outdent();
  printer->Print("}\n");
}

void ImmutableMessageFieldGenerator::PrintNestedBuilderFunction(
    io::Printer* printer, const char* method_prototype,
    const char* regular_case, const char* nested_builder_case,
    const char* trailing_code) const {
  printer->Print(variables_, method_prototype);
  printer->Annotate("{", "}", descriptor_);
  printer->Print(" {\n");
  printer->Indent();
  PrintNestedBuilderCondition(printer, regular_case, nested_builder_case);
  if (trailing_code != NULL) {
    printer->Print(variables_, trailing_code);
  }
  printer->Outdent();
  printer->Print("}\n");
}

void ImmutableMessageFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  // When using nested-builders, the code initially works just like the
  // non-nested builder case. It only creates a nested builder lazily on
  // demand and then forever delegates to it after creation.

  bool support_field_presence = SupportFieldPresence(descriptor_->file());

  printer->Print(variables_, "private $type$ $name$_;\n");

  printer->Print(variables_,
                 // If this builder is non-null, it is used and the other fields
                 // are ignored.
                 "private com.google.protobuf.SingleFieldBuilder$ver$<\n"
                 "    $type$, $type$.Builder, $type$OrBuilder> $name$Builder_;"
                 "\n");

  // The comments above the methods below are based on a hypothetical
  // field of type "Field" called "Field".

  // boolean hasField()
  WriteFieldDocComment(printer, descriptor_);
  if (support_field_presence) {
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $get_has_field_bit_builder$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  } else {
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $name$Builder_ != null || $name$_ != null;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  // Field getField()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public $type$ ${$get$capitalized_name$$}$()",
      "return $name$_ == null ? $type$.getDefaultInstance() : $name$_;\n",
      "return $name$Builder_.getMessage();\n", NULL);

  // Field.Builder setField(Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$($type$ value)",

      "if (value == null) {\n"
      "  throw new NullPointerException();\n"
      "}\n"
      "$name$_ = value;\n"
      "$on_changed$\n",

      "$name$Builder_.setMessage(value);\n",

      "$set_has_field_bit_builder$\n"
      "return this;\n");

  // Field.Builder setField(Field.Builder builderForValue)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
      "    $type$.Builder builderForValue)",

      "$name$_ = builderForValue.build();\n"
      "$on_changed$\n",

      "$name$Builder_.setMessage(builderForValue.build());\n",

      "$set_has_field_bit_builder$\n"
      "return this;\n");

  // Field.Builder mergeField(Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$merge$capitalized_name$$}$($type$ value)",

      support_field_presence
          ? "if ($get_has_field_bit_builder$ &&\n"
            "    $name$_ != null &&\n"
            "    $name$_ != $type$.getDefaultInstance()) {\n"
            "  $name$_ =\n"
            "    $type$.newBuilder($name$_).mergeFrom(value).buildPartial();\n"
            "} else {\n"
            "  $name$_ = value;\n"
            "}\n"
            "$on_changed$\n"
          : "if ($name$_ != null) {\n"
            "  $name$_ =\n"
            "    $type$.newBuilder($name$_).mergeFrom(value).buildPartial();\n"
            "} else {\n"
            "  $name$_ = value;\n"
            "}\n"
            "$on_changed$\n",

      "$name$Builder_.mergeFrom(value);\n",

      "$set_has_field_bit_builder$\n"
      "return this;\n");

  // Field.Builder clearField()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public Builder ${$clear$capitalized_name$$}$()",

      "$name$_ = null;\n"
      "$on_changed$\n",

      support_field_presence ? "$name$Builder_.clear();\n"
                             : "$name$_ = null;\n"
                               "$name$Builder_ = null;\n",

      "$clear_has_field_bit_builder$\n"
      "return this;\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$.Builder "
                 "${$get$capitalized_name$Builder$}$() {\n"
                 "  $set_has_field_bit_builder$\n"
                 "  $on_changed$\n"
                 "  return get$capitalized_name$FieldBuilder().getBuilder();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$OrBuilder "
                 "${$get$capitalized_name$OrBuilder$}$() {\n"
                 "  if ($name$Builder_ != null) {\n"
                 "    return $name$Builder_.getMessageOrBuilder();\n"
                 "  } else {\n"
                 "    return $name$_ == null ?\n"
                 "        $type$.getDefaultInstance() : $name$_;\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "private com.google.protobuf.SingleFieldBuilder$ver$<\n"
      "    $type$, $type$.Builder, $type$OrBuilder> \n"
      "    get$capitalized_name$FieldBuilder() {\n"
      "  if ($name$Builder_ == null) {\n"
      "    $name$Builder_ = new com.google.protobuf.SingleFieldBuilder$ver$<\n"
      "        $type$, $type$.Builder, $type$OrBuilder>(\n"
      "            get$capitalized_name$(),\n"
      "            getParentForChildren(),\n"
      "            isClean());\n"
      "    $name$_ = null;\n"
      "  }\n"
      "  return $name$Builder_;\n"
      "}\n");
}

void ImmutableMessageFieldGenerator::GenerateFieldBuilderInitializationCode(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    printer->Print(variables_, "get$capitalized_name$FieldBuilder();\n");
  }
}

void ImmutableMessageFieldGenerator::GenerateInitializationCode(
    io::Printer* printer) const {}

void ImmutableMessageFieldGenerator::GenerateBuilderClearCode(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    PrintNestedBuilderCondition(printer, "$name$_ = null;\n",

                                "$name$Builder_.clear();\n");
    printer->Print(variables_, "$clear_has_field_bit_builder$\n");
  } else {
    PrintNestedBuilderCondition(printer, "$name$_ = null;\n",

                                "$name$_ = null;\n"
                                "$name$Builder_ = null;\n");
  }
}

void ImmutableMessageFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (other.has$capitalized_name$()) {\n"
                 "  merge$capitalized_name$(other.get$capitalized_name$());\n"
                 "}\n");
}

void ImmutableMessageFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    printer->Print(variables_, "if ($get_has_field_bit_from_local$) {\n");
    printer->Indent();
    PrintNestedBuilderCondition(printer, "result.$name$_ = $name$_;\n",
                                "result.$name$_ = $name$Builder_.build();\n");
    printer->Outdent();
    printer->Print(variables_,
                   "  $set_has_field_bit_to_local$;\n"
                   "}\n");
  } else {
    PrintNestedBuilderCondition(printer, "result.$name$_ = $name$_;\n",
                                "result.$name$_ = $name$Builder_.build();\n");
  }
}

void ImmutableMessageFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "$type$.Builder subBuilder = null;\n"
                 "if ($is_field_present_message$) {\n"
                 "  subBuilder = $name$_.toBuilder();\n"
                 "}\n");

  if (GetType(descriptor_) == FieldDescriptor::TYPE_GROUP) {
    printer->Print(variables_,
                   "$name$_ = input.readGroup($number$, $type$.$get_parser$,\n"
                   "    extensionRegistry);\n");
  } else {
    printer->Print(variables_,
                   "$name$_ = input.readMessage($type$.$get_parser$, "
                   "extensionRegistry);\n");
  }

  printer->Print(variables_,
                 "if (subBuilder != null) {\n"
                 "  subBuilder.mergeFrom($name$_);\n"
                 "  $name$_ = subBuilder.buildPartial();\n"
                 "}\n"
                 "$set_has_field_bit_message$\n");
}

void ImmutableMessageFieldGenerator::GenerateParsingDoneCode(
    io::Printer* printer) const {
  // noop for messages.
}

void ImmutableMessageFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if ($is_field_present_message$) {\n"
      "  output.write$group_or_message$($number$, get$capitalized_name$());\n"
      "}\n");
}

void ImmutableMessageFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if ($is_field_present_message$) {\n"
      "  size += com.google.protobuf.CodedOutputStream\n"
      "    .compute$group_or_message$Size($number$, get$capitalized_name$());\n"
      "}\n");
}

void ImmutableMessageFieldGenerator::GenerateEqualsCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (!get$capitalized_name$()\n"
                 "    .equals(other.get$capitalized_name$())) return false;\n");
}

void ImmutableMessageFieldGenerator::GenerateHashCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "hash = (37 * hash) + $constant_name$;\n"
                 "hash = (53 * hash) + get$capitalized_name$().hashCode();\n");
}

std::string ImmutableMessageFieldGenerator::GetBoxedType() const {
  return name_resolver_->GetImmutableClassName(descriptor_->message_type());
}

// ===================================================================

ImmutableMessageOneofFieldGenerator::ImmutableMessageOneofFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : ImmutableMessageFieldGenerator(descriptor, messageBitIndex,
                                     builderBitIndex, context) {
  const OneofGeneratorInfo* info =
      context->GetOneofGeneratorInfo(descriptor->containing_oneof());
  SetCommonOneofVariables(descriptor, info, &variables_);
}

ImmutableMessageOneofFieldGenerator::~ImmutableMessageOneofFieldGenerator() {}

void ImmutableMessageOneofFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  PrintExtraFieldInfo(variables_, printer);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
                 "  return $has_oneof_case_message$;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "     return ($type$) $oneof_name$_;\n"
                 "  }\n"
                 "  return $type$.getDefaultInstance();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$OrBuilder "
                 "${$get$capitalized_name$OrBuilder$}$() {\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "     return ($type$) $oneof_name$_;\n"
                 "  }\n"
                 "  return $type$.getDefaultInstance();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void ImmutableMessageOneofFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  // When using nested-builders, the code initially works just like the
  // non-nested builder case. It only creates a nested builder lazily on
  // demand and then forever delegates to it after creation.
  printer->Print(variables_,
                 // If this builder is non-null, it is used and the other fields
                 // are ignored.
                 "private com.google.protobuf.SingleFieldBuilder$ver$<\n"
                 "    $type$, $type$.Builder, $type$OrBuilder> $name$Builder_;"
                 "\n");

  // The comments above the methods below are based on a hypothetical
  // field of type "Field" called "Field".

  // boolean hasField()
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
                 "  return $has_oneof_case_message$;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  // Field getField()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public $type$ ${$get$capitalized_name$$}$()",

      "if ($has_oneof_case_message$) {\n"
      "  return ($type$) $oneof_name$_;\n"
      "}\n"
      "return $type$.getDefaultInstance();\n",

      "if ($has_oneof_case_message$) {\n"
      "  return $name$Builder_.getMessage();\n"
      "}\n"
      "return $type$.getDefaultInstance();\n",

      NULL);

  // Field.Builder setField(Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$($type$ value)",

      "if (value == null) {\n"
      "  throw new NullPointerException();\n"
      "}\n"
      "$oneof_name$_ = value;\n"
      "$on_changed$\n",

      "$name$Builder_.setMessage(value);\n",

      "$set_oneof_case_message$;\n"
      "return this;\n");

  // Field.Builder setField(Field.Builder builderForValue)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
      "    $type$.Builder builderForValue)",

      "$oneof_name$_ = builderForValue.build();\n"
      "$on_changed$\n",

      "$name$Builder_.setMessage(builderForValue.build());\n",

      "$set_oneof_case_message$;\n"
      "return this;\n");

  // Field.Builder mergeField(Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$merge$capitalized_name$$}$($type$ value)",

      "if ($has_oneof_case_message$ &&\n"
      "    $oneof_name$_ != $type$.getDefaultInstance()) {\n"
      "  $oneof_name$_ = $type$.newBuilder(($type$) $oneof_name$_)\n"
      "      .mergeFrom(value).buildPartial();\n"
      "} else {\n"
      "  $oneof_name$_ = value;\n"
      "}\n"
      "$on_changed$\n",

      "if ($has_oneof_case_message$) {\n"
      "  $name$Builder_.mergeFrom(value);\n"
      "}\n"
      "$name$Builder_.setMessage(value);\n",

      "$set_oneof_case_message$;\n"
      "return this;\n");

  // Field.Builder clearField()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public Builder ${$clear$capitalized_name$$}$()",

      "if ($has_oneof_case_message$) {\n"
      "  $clear_oneof_case_message$;\n"
      "  $oneof_name$_ = null;\n"
      "  $on_changed$\n"
      "}\n",

      "if ($has_oneof_case_message$) {\n"
      "  $clear_oneof_case_message$;\n"
      "  $oneof_name$_ = null;\n"
      "}\n"
      "$name$Builder_.clear();\n",

      "return this;\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$.Builder "
                 "${$get$capitalized_name$Builder$}$() {\n"
                 "  return get$capitalized_name$FieldBuilder().getBuilder();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public $type$OrBuilder "
      "${$get$capitalized_name$OrBuilder$}$() {\n"
      "  if (($has_oneof_case_message$) && ($name$Builder_ != null)) {\n"
      "    return $name$Builder_.getMessageOrBuilder();\n"
      "  } else {\n"
      "    if ($has_oneof_case_message$) {\n"
      "      return ($type$) $oneof_name$_;\n"
      "    }\n"
      "    return $type$.getDefaultInstance();\n"
      "  }\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "private com.google.protobuf.SingleFieldBuilder$ver$<\n"
      "    $type$, $type$.Builder, $type$OrBuilder> \n"
      "    ${$get$capitalized_name$FieldBuilder$}$() {\n"
      "  if ($name$Builder_ == null) {\n"
      "    if (!($has_oneof_case_message$)) {\n"
      "      $oneof_name$_ = $type$.getDefaultInstance();\n"
      "    }\n"
      "    $name$Builder_ = new com.google.protobuf.SingleFieldBuilder$ver$<\n"
      "        $type$, $type$.Builder, $type$OrBuilder>(\n"
      "            ($type$) $oneof_name$_,\n"
      "            getParentForChildren(),\n"
      "            isClean());\n"
      "    $oneof_name$_ = null;\n"
      "  }\n"
      "  $set_oneof_case_message$;\n"
      "  $on_changed$;\n"
      "  return $name$Builder_;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void ImmutableMessageOneofFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  printer->Print(variables_, "if ($has_oneof_case_message$) {\n");
  printer->Indent();

  PrintNestedBuilderCondition(
      printer, "result.$oneof_name$_ = $oneof_name$_;\n",

      "result.$oneof_name$_ = $name$Builder_.build();\n");

  printer->Outdent();
  printer->Print("}\n");
}

void ImmutableMessageOneofFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "merge$capitalized_name$(other.get$capitalized_name$());\n");
}

void ImmutableMessageOneofFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "$type$.Builder subBuilder = null;\n"
                 "if ($has_oneof_case_message$) {\n"
                 "  subBuilder = (($type$) $oneof_name$_).toBuilder();\n"
                 "}\n");

  if (GetType(descriptor_) == FieldDescriptor::TYPE_GROUP) {
    printer->Print(
        variables_,
        "$oneof_name$_ = input.readGroup($number$, $type$.$get_parser$,\n"
        "    extensionRegistry);\n");
  } else {
    printer->Print(
        variables_,
        "$oneof_name$_ =\n"
        "    input.readMessage($type$.$get_parser$, extensionRegistry);\n");
  }

  printer->Print(variables_,
                 "if (subBuilder != null) {\n"
                 "  subBuilder.mergeFrom(($type$) $oneof_name$_);\n"
                 "  $oneof_name$_ = subBuilder.buildPartial();\n"
                 "}\n");
  printer->Print(variables_, "$set_oneof_case_message$;\n");
}

void ImmutableMessageOneofFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if ($has_oneof_case_message$) {\n"
      "  output.write$group_or_message$($number$, ($type$) $oneof_name$_);\n"
      "}\n");
}

void ImmutableMessageOneofFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if ($has_oneof_case_message$) {\n"
      "  size += com.google.protobuf.CodedOutputStream\n"
      "    .compute$group_or_message$Size($number$, ($type$) $oneof_name$_);\n"
      "}\n");
}

// ===================================================================

RepeatedImmutableMessageFieldGenerator::RepeatedImmutableMessageFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  SetMessageVariables(descriptor, messageBitIndex, builderBitIndex,
                      context->GetFieldGeneratorInfo(descriptor),
                      name_resolver_, &variables_);
}

RepeatedImmutableMessageFieldGenerator::
    ~RepeatedImmutableMessageFieldGenerator() {}

int RepeatedImmutableMessageFieldGenerator::GetNumBitsForMessage() const {
  return 0;
}

int RepeatedImmutableMessageFieldGenerator::GetNumBitsForBuilder() const {
  return 1;
}

void RepeatedImmutableMessageFieldGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  // TODO(jonp): In the future, consider having methods specific to the
  // interface so that builders can choose dynamically to either return a
  // message or a nested builder, so that asking for the interface doesn't
  // cause a message to ever be built.
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$java.util.List<$type$> \n"
                 "    get$capitalized_name$List();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$$type$ get$capitalized_name$(int index);\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$int get$capitalized_name$Count();\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$java.util.List<? extends $type$OrBuilder> \n"
                 "    get$capitalized_name$OrBuilderList();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$$type$OrBuilder get$capitalized_name$OrBuilder(\n"
      "    int index);\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  printer->Print(variables_, "private java.util.List<$type$> $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public java.util.List<$type$> "
                 "${$get$capitalized_name$List$}$() {\n"
                 "  return $name$_;\n"  // note:  unmodifiable list
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.util.List<? extends $type$OrBuilder> \n"
      "    ${$get$capitalized_name$OrBuilderList$}$() {\n"
      "  return $name$_;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public int ${$get$capitalized_name$Count$}$() {\n"
      "  return $name$_.size();\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public $type$ ${$get$capitalized_name$$}$(int index) {\n"
      "  return $name$_.get(index);\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$OrBuilder "
                 "${$get$capitalized_name$OrBuilder$}$(\n"
                 "    int index) {\n"
                 "  return $name$_.get(index);\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void RepeatedImmutableMessageFieldGenerator::PrintNestedBuilderCondition(
    io::Printer* printer, const char* regular_case,
    const char* nested_builder_case) const {
  printer->Print(variables_, "if ($name$Builder_ == null) {\n");
  printer->Indent();
  printer->Print(variables_, regular_case);
  printer->Outdent();
  printer->Print("} else {\n");
  printer->Indent();
  printer->Print(variables_, nested_builder_case);
  printer->Outdent();
  printer->Print("}\n");
}

void RepeatedImmutableMessageFieldGenerator::PrintNestedBuilderFunction(
    io::Printer* printer, const char* method_prototype,
    const char* regular_case, const char* nested_builder_case,
    const char* trailing_code) const {
  printer->Print(variables_, method_prototype);
  printer->Annotate("{", "}", descriptor_);
  printer->Print(" {\n");
  printer->Indent();
  PrintNestedBuilderCondition(printer, regular_case, nested_builder_case);
  if (trailing_code != NULL) {
    printer->Print(variables_, trailing_code);
  }
  printer->Outdent();
  printer->Print("}\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  // When using nested-builders, the code initially works just like the
  // non-nested builder case. It only creates a nested builder lazily on
  // demand and then forever delegates to it after creation.

  printer->Print(
      variables_,
      // Used when the builder is null.
      // One field is the list and the other field keeps track of whether the
      // list is immutable. If it's immutable, the invariant is that it must
      // either an instance of Collections.emptyList() or it's an ArrayList
      // wrapped in a Collections.unmodifiableList() wrapper and nobody else has
      // a refererence to the underlying ArrayList. This invariant allows us to
      // share instances of lists between protocol buffers avoiding expensive
      // memory allocations. Note, immutable is a strong guarantee here -- not
      // just that the list cannot be modified via the reference but that the
      // list can never be modified.
      "private java.util.List<$type$> $name$_ =\n"
      "  java.util.Collections.emptyList();\n"

      "private void ensure$capitalized_name$IsMutable() {\n"
      "  if (!$get_mutable_bit_builder$) {\n"
      "    $name$_ = new java.util.ArrayList<$type$>($name$_);\n"
      "    $set_mutable_bit_builder$;\n"
      "   }\n"
      "}\n"
      "\n");

  printer->Print(
      variables_,
      // If this builder is non-null, it is used and the other fields are
      // ignored.
      "private com.google.protobuf.RepeatedFieldBuilder$ver$<\n"
      "    $type$, $type$.Builder, $type$OrBuilder> $name$Builder_;\n"
      "\n");

  // The comments above the methods below are based on a hypothetical
  // repeated field of type "Field" called "RepeatedField".

  // List<Field> getRepeatedFieldList()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public java.util.List<$type$> "
      "${$get$capitalized_name$List$}$()",

      "return java.util.Collections.unmodifiableList($name$_);\n",
      "return $name$Builder_.getMessageList();\n",

      NULL);

  // int getRepeatedFieldCount()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public int ${$get$capitalized_name$Count$}$()",

      "return $name$_.size();\n", "return $name$Builder_.getCount();\n",

      NULL);

  // Field getRepeatedField(int index)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public $type$ ${$get$capitalized_name$$}$(int index)",

      "return $name$_.get(index);\n",

      "return $name$Builder_.getMessage(index);\n",

      NULL);

  // Builder setRepeatedField(int index, Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
      "    int index, $type$ value)",
      "if (value == null) {\n"
      "  throw new NullPointerException();\n"
      "}\n"
      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.set(index, value);\n"
      "$on_changed$\n",
      "$name$Builder_.setMessage(index, value);\n", "return this;\n");

  // Builder setRepeatedField(int index, Field.Builder builderForValue)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
      "    int index, $type$.Builder builderForValue)",

      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.set(index, builderForValue.build());\n"
      "$on_changed$\n",

      "$name$Builder_.setMessage(index, builderForValue.build());\n",

      "return this;\n");

  // Builder addRepeatedField(Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$add$capitalized_name$$}$($type$ value)",

      "if (value == null) {\n"
      "  throw new NullPointerException();\n"
      "}\n"
      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.add(value);\n"

      "$on_changed$\n",

      "$name$Builder_.addMessage(value);\n",

      "return this;\n");

  // Builder addRepeatedField(int index, Field value)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$add$capitalized_name$$}$(\n"
      "    int index, $type$ value)",

      "if (value == null) {\n"
      "  throw new NullPointerException();\n"
      "}\n"
      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.add(index, value);\n"
      "$on_changed$\n",

      "$name$Builder_.addMessage(index, value);\n",

      "return this;\n");

  // Builder addRepeatedField(Field.Builder builderForValue)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$add$capitalized_name$$}$(\n"
      "    $type$.Builder builderForValue)",

      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.add(builderForValue.build());\n"
      "$on_changed$\n",

      "$name$Builder_.addMessage(builderForValue.build());\n",

      "return this;\n");

  // Builder addRepeatedField(int index, Field.Builder builderForValue)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$add$capitalized_name$$}$(\n"
      "    int index, $type$.Builder builderForValue)",

      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.add(index, builderForValue.build());\n"
      "$on_changed$\n",

      "$name$Builder_.addMessage(index, builderForValue.build());\n",

      "return this;\n");

  // Builder addAllRepeatedField(Iterable<Field> values)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$addAll$capitalized_name$$}$(\n"
      "    java.lang.Iterable<? extends $type$> values)",

      "ensure$capitalized_name$IsMutable();\n"
      "com.google.protobuf.AbstractMessageLite.Builder.addAll(\n"
      "    values, $name$_);\n"
      "$on_changed$\n",

      "$name$Builder_.addAllMessages(values);\n",

      "return this;\n");

  // Builder clearAllRepeatedField()
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer, "$deprecation$public Builder ${$clear$capitalized_name$$}$()",

      "$name$_ = java.util.Collections.emptyList();\n"
      "$clear_mutable_bit_builder$;\n"
      "$on_changed$\n",

      "$name$Builder_.clear();\n",

      "return this;\n");

  // Builder removeRepeatedField(int index)
  WriteFieldDocComment(printer, descriptor_);
  PrintNestedBuilderFunction(
      printer,
      "$deprecation$public Builder ${$remove$capitalized_name$$}$(int index)",

      "ensure$capitalized_name$IsMutable();\n"
      "$name$_.remove(index);\n"
      "$on_changed$\n",

      "$name$Builder_.remove(index);\n",

      "return this;\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public $type$.Builder ${$get$capitalized_name$Builder$}$(\n"
      "    int index) {\n"
      "  return get$capitalized_name$FieldBuilder().getBuilder(index);\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$OrBuilder "
                 "${$get$capitalized_name$OrBuilder$}$(\n"
                 "    int index) {\n"
                 "  if ($name$Builder_ == null) {\n"
                 "    return $name$_.get(index);"
                 "  } else {\n"
                 "    return $name$Builder_.getMessageOrBuilder(index);\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.util.List<? extends $type$OrBuilder> \n"
      "     ${$get$capitalized_name$OrBuilderList$}$() {\n"
      "  if ($name$Builder_ != null) {\n"
      "    return $name$Builder_.getMessageOrBuilderList();\n"
      "  } else {\n"
      "    return java.util.Collections.unmodifiableList($name$_);\n"
      "  }\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public $type$.Builder "
                 "${$add$capitalized_name$Builder$}$() {\n"
                 "  return get$capitalized_name$FieldBuilder().addBuilder(\n"
                 "      $type$.getDefaultInstance());\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public $type$.Builder ${$add$capitalized_name$Builder$}$(\n"
      "    int index) {\n"
      "  return get$capitalized_name$FieldBuilder().addBuilder(\n"
      "      index, $type$.getDefaultInstance());\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.util.List<$type$.Builder> \n"
      "     ${$get$capitalized_name$BuilderList$}$() {\n"
      "  return get$capitalized_name$FieldBuilder().getBuilderList();\n"
      "}\n"
      "private com.google.protobuf.RepeatedFieldBuilder$ver$<\n"
      "    $type$, $type$.Builder, $type$OrBuilder> \n"
      "    get$capitalized_name$FieldBuilder() {\n"
      "  if ($name$Builder_ == null) {\n"
      "    $name$Builder_ = new "
      "com.google.protobuf.RepeatedFieldBuilder$ver$<\n"
      "        $type$, $type$.Builder, $type$OrBuilder>(\n"
      "            $name$_,\n"
      "            $get_mutable_bit_builder$,\n"
      "            getParentForChildren(),\n"
      "            isClean());\n"
      "    $name$_ = null;\n"
      "  }\n"
      "  return $name$Builder_;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void RepeatedImmutableMessageFieldGenerator::
    GenerateFieldBuilderInitializationCode(io::Printer* printer) const {
  printer->Print(variables_, "get$capitalized_name$FieldBuilder();\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  printer->Print(variables_, "$name$_ = java.util.Collections.emptyList();\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateBuilderClearCode(
    io::Printer* printer) const {
  PrintNestedBuilderCondition(printer,
                              "$name$_ = java.util.Collections.emptyList();\n"
                              "$clear_mutable_bit_builder$;\n",

                              "$name$Builder_.clear();\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  // The code below does two optimizations (non-nested builder case):
  //   1. If the other list is empty, there's nothing to do. This ensures we
  //      don't allocate a new array if we already have an immutable one.
  //   2. If the other list is non-empty and our current list is empty, we can
  //      reuse the other list which is guaranteed to be immutable.
  PrintNestedBuilderCondition(
      printer,
      "if (!other.$name$_.isEmpty()) {\n"
      "  if ($name$_.isEmpty()) {\n"
      "    $name$_ = other.$name$_;\n"
      "    $clear_mutable_bit_builder$;\n"
      "  } else {\n"
      "    ensure$capitalized_name$IsMutable();\n"
      "    $name$_.addAll(other.$name$_);\n"
      "  }\n"
      "  $on_changed$\n"
      "}\n",

      "if (!other.$name$_.isEmpty()) {\n"
      "  if ($name$Builder_.isEmpty()) {\n"
      "    $name$Builder_.dispose();\n"
      "    $name$Builder_ = null;\n"
      "    $name$_ = other.$name$_;\n"
      "    $clear_mutable_bit_builder$;\n"
      "    $name$Builder_ = \n"
      "      com.google.protobuf.GeneratedMessage$ver$.alwaysUseFieldBuilders "
      "?\n"
      "         get$capitalized_name$FieldBuilder() : null;\n"
      "  } else {\n"
      "    $name$Builder_.addAllMessages(other.$name$_);\n"
      "  }\n"
      "}\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  // The code below (non-nested builder case) ensures that the result has an
  // immutable list. If our list is immutable, we can just reuse it. If not,
  // we make it immutable.
  PrintNestedBuilderCondition(
      printer,
      "if ($get_mutable_bit_builder$) {\n"
      "  $name$_ = java.util.Collections.unmodifiableList($name$_);\n"
      "  $clear_mutable_bit_builder$;\n"
      "}\n"
      "result.$name$_ = $name$_;\n",

      "result.$name$_ = $name$Builder_.build();\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (!$get_mutable_bit_parser$) {\n"
                 "  $name$_ = new java.util.ArrayList<$type$>();\n"
                 "  $set_mutable_bit_parser$;\n"
                 "}\n");

  if (GetType(descriptor_) == FieldDescriptor::TYPE_GROUP) {
    printer->Print(
        variables_,
        "$name$_.add(input.readGroup($number$, $type$.$get_parser$,\n"
        "    extensionRegistry));\n");
  } else {
    printer->Print(
        variables_,
        "$name$_.add(\n"
        "    input.readMessage($type$.$get_parser$, extensionRegistry));\n");
  }
}

void RepeatedImmutableMessageFieldGenerator::GenerateParsingDoneCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if ($get_mutable_bit_parser$) {\n"
      "  $name$_ = java.util.Collections.unmodifiableList($name$_);\n"
      "}\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "for (int i = 0; i < $name$_.size(); i++) {\n"
                 "  output.write$group_or_message$($number$, $name$_.get(i));\n"
                 "}\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  size += com.google.protobuf.CodedOutputStream\n"
      "    .compute$group_or_message$Size($number$, $name$_.get(i));\n"
      "}\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateEqualsCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if (!get$capitalized_name$List()\n"
      "    .equals(other.get$capitalized_name$List())) return false;\n");
}

void RepeatedImmutableMessageFieldGenerator::GenerateHashCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if (get$capitalized_name$Count() > 0) {\n"
      "  hash = (37 * hash) + $constant_name$;\n"
      "  hash = (53 * hash) + get$capitalized_name$List().hashCode();\n"
      "}\n");
}

std::string RepeatedImmutableMessageFieldGenerator::GetBoxedType() const {
  return name_resolver_->GetImmutableClassName(descriptor_->message_type());
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
