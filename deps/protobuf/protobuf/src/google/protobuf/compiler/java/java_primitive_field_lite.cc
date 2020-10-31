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

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/compiler/java/java_primitive_field_lite.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace java {

using internal::WireFormat;
using internal::WireFormatLite;

namespace {
bool EnableExperimentalRuntimeForLite() {
#ifdef PROTOBUF_EXPERIMENT
  return PROTOBUF_EXPERIMENT;
#else   // PROTOBUF_EXPERIMENT
  return false;
#endif  // !PROTOBUF_EXPERIMENT
}

void SetPrimitiveVariables(const FieldDescriptor* descriptor,
                           int messageBitIndex, int builderBitIndex,
                           const FieldGeneratorInfo* info,
                           ClassNameResolver* name_resolver,
                           std::map<std::string, std::string>* variables) {
  SetCommonFieldVariables(descriptor, info, variables);
  JavaType javaType = GetJavaType(descriptor);
  (*variables)["type"] = PrimitiveTypeName(javaType);
  (*variables)["boxed_type"] = BoxedPrimitiveTypeName(javaType);
  (*variables)["field_type"] = (*variables)["type"];
  (*variables)["default"] = ImmutableDefaultValue(descriptor, name_resolver);
  (*variables)["capitalized_type"] =
      GetCapitalizedType(descriptor, /* immutable = */ true);
  (*variables)["tag"] =
      StrCat(static_cast<int32>(WireFormat::MakeTag(descriptor)));
  (*variables)["tag_size"] = StrCat(
      WireFormat::TagSize(descriptor->number(), GetType(descriptor)));
  (*variables)["required"] = descriptor->is_required() ? "true" : "false";

  std::string capitalized_type = UnderscoresToCamelCase(
      PrimitiveTypeName(javaType), true /* cap_next_letter */);
  switch (javaType) {
    case JAVATYPE_INT:
    case JAVATYPE_LONG:
    case JAVATYPE_FLOAT:
    case JAVATYPE_DOUBLE:
    case JAVATYPE_BOOLEAN:
      (*variables)["field_list_type"] =
          "com.google.protobuf.Internal." + capitalized_type + "List";
      (*variables)["empty_list"] = "empty" + capitalized_type + "List()";
      (*variables)["make_name_unmodifiable"] =
          (*variables)["name"] + "_.makeImmutable()";
      (*variables)["repeated_get"] =
          (*variables)["name"] + "_.get" + capitalized_type;
      (*variables)["repeated_add"] =
          (*variables)["name"] + "_.add" + capitalized_type;
      (*variables)["repeated_set"] =
          (*variables)["name"] + "_.set" + capitalized_type;
      (*variables)["visit_type"] = capitalized_type;
      (*variables)["visit_type_list"] = "visit" + capitalized_type + "List";
      break;
    default:
      (*variables)["field_list_type"] =
          "com.google.protobuf.Internal.ProtobufList<" +
          (*variables)["boxed_type"] + ">";
      (*variables)["empty_list"] = "emptyProtobufList()";
      (*variables)["make_name_unmodifiable"] =
          (*variables)["name"] + "_.makeImmutable()";
      (*variables)["repeated_get"] = (*variables)["name"] + "_.get";
      (*variables)["repeated_add"] = (*variables)["name"] + "_.add";
      (*variables)["repeated_set"] = (*variables)["name"] + "_.set";
      (*variables)["visit_type"] = "ByteString";
      (*variables)["visit_type_list"] = "visitList";
  }

  if (javaType == JAVATYPE_BYTES) {
    (*variables)["bytes_default"] =
        ToUpper((*variables)["name"]) + "_DEFAULT_VALUE";
  }

  if (IsReferenceType(javaType)) {
    (*variables)["null_check"] =
        "  if (value == null) {\n"
        "    throw new NullPointerException();\n"
        "  }\n";
  } else {
    (*variables)["null_check"] = "";
  }
  // TODO(birdo): Add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] =
      descriptor->options().deprecated() ? "@java.lang.Deprecated " : "";
  int fixed_size = FixedSize(GetType(descriptor));
  if (fixed_size != -1) {
    (*variables)["fixed_size"] = StrCat(fixed_size);
  }

  if (SupportFieldPresence(descriptor->file())) {
    // For singular messages and builders, one bit is used for the hasField bit.
    (*variables)["get_has_field_bit_message"] = GenerateGetBit(messageBitIndex);

    // Note that these have a trailing ";".
    (*variables)["set_has_field_bit_message"] =
        GenerateSetBit(messageBitIndex) + ";";
    (*variables)["clear_has_field_bit_message"] =
        GenerateClearBit(messageBitIndex) + ";";

    (*variables)["is_field_present_message"] = GenerateGetBit(messageBitIndex);
  } else {
    (*variables)["set_has_field_bit_message"] = "";
    (*variables)["set_has_field_bit_message"] = "";
    (*variables)["clear_has_field_bit_message"] = "";

    if (descriptor->type() == FieldDescriptor::TYPE_BYTES) {
      (*variables)["is_field_present_message"] =
          "!" + (*variables)["name"] + "_.isEmpty()";
    } else {
      (*variables)["is_field_present_message"] =
          (*variables)["name"] + "_ != " + (*variables)["default"];
    }
  }

  // For repeated builders, the underlying list tracks mutability state.
  (*variables)["is_mutable"] = (*variables)["name"] + "_.isModifiable()";

  (*variables)["get_has_field_bit_from_local"] =
      GenerateGetBitFromLocal(builderBitIndex);
  (*variables)["set_has_field_bit_to_local"] =
      GenerateSetBitToLocal(messageBitIndex);
}

}  // namespace

// ===================================================================

ImmutablePrimitiveFieldLiteGenerator::ImmutablePrimitiveFieldLiteGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, Context* context)
    : descriptor_(descriptor),
      messageBitIndex_(messageBitIndex),
      name_resolver_(context->GetNameResolver()) {
  SetPrimitiveVariables(descriptor, messageBitIndex, 0,
                        context->GetFieldGeneratorInfo(descriptor),
                        name_resolver_, &variables_);
}

ImmutablePrimitiveFieldLiteGenerator::~ImmutablePrimitiveFieldLiteGenerator() {}

int ImmutablePrimitiveFieldLiteGenerator::GetNumBitsForMessage() const {
  return SupportFieldPresence(descriptor_->file()) ? 1 : 0;
}

void ImmutablePrimitiveFieldLiteGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$boolean has$capitalized_name$();\n");
  }
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_, "$deprecation$$type$ get$capitalized_name$();\n");
}

void ImmutablePrimitiveFieldLiteGenerator::GenerateMembers(
    io::Printer* printer) const {
  if (IsByteStringWithCustomDefaultValue(descriptor_)) {
    // allocate this once statically since we know ByteStrings are immutable
    // values that can be reused.
    printer->Print(
        variables_,
        "private static final $field_type$ $bytes_default$ = $default$;\n");
  }
  printer->Print(variables_, "private $field_type$ $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "@java.lang.Override\n"
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $get_has_field_bit_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
                 "  return $name$_;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void set$capitalized_name$($type$ value) {\n"
                 "$null_check$"
                 "  $set_has_field_bit_message$\n"
                 "  $name$_ = value;\n"
                 "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void clear$capitalized_name$() {\n"
                 "  $clear_has_field_bit_message$\n");
  JavaType type = GetJavaType(descriptor_);
  if (type == JAVATYPE_STRING || type == JAVATYPE_BYTES) {
    // The default value is not a simple literal so we want to avoid executing
    // it multiple times.  Instead, get the default out of the default instance.
    printer->Print(
        variables_,
        "  $name$_ = getDefaultInstance().get$capitalized_name$();\n");
  } else {
    printer->Print(variables_, "  $name$_ = $default$;\n");
  }
  printer->Print(variables_, "}\n");
}

void ImmutablePrimitiveFieldLiteGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "@java.lang.Override\n"
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return instance.has$capitalized_name$();\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
                 "  return instance.get$capitalized_name$();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder "
                 "${$set$capitalized_name$$}$($type$ value) {\n"
                 "  copyOnWrite();\n"
                 "  instance.set$capitalized_name$(value);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  copyOnWrite();\n"
      "  instance.clear$capitalized_name$();\n"
      "  return this;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void ImmutablePrimitiveFieldLiteGenerator::GenerateFieldInfo(
    io::Printer* printer, std::vector<uint16>* output) const {
  WriteIntToUtf16CharSequence(descriptor_->number(), output);
  WriteIntToUtf16CharSequence(GetExperimentalJavaFieldType(descriptor_),
                              output);
  if (SupportFieldPresence(descriptor_->file())) {
    WriteIntToUtf16CharSequence(messageBitIndex_, output);
  }
  printer->Print(variables_, "\"$name$_\",\n");
}

void ImmutablePrimitiveFieldLiteGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  if (IsByteStringWithCustomDefaultValue(descriptor_)) {
    printer->Print(variables_, "$name$_ = $bytes_default$;\n");
  } else if (!IsDefaultValueJavaDefault(descriptor_)) {
    printer->Print(variables_, "$name$_ = $default$;\n");
  }
}

std::string ImmutablePrimitiveFieldLiteGenerator::GetBoxedType() const {
  return BoxedPrimitiveTypeName(GetJavaType(descriptor_));
}

// ===================================================================

ImmutablePrimitiveOneofFieldLiteGenerator::
    ImmutablePrimitiveOneofFieldLiteGenerator(const FieldDescriptor* descriptor,
                                              int messageBitIndex,
                                              Context* context)
    : ImmutablePrimitiveFieldLiteGenerator(descriptor, messageBitIndex,
                                           context) {
  const OneofGeneratorInfo* info =
      context->GetOneofGeneratorInfo(descriptor->containing_oneof());
  SetCommonOneofVariables(descriptor, info, &variables_);
}

ImmutablePrimitiveOneofFieldLiteGenerator::
    ~ImmutablePrimitiveOneofFieldLiteGenerator() {}

void ImmutablePrimitiveOneofFieldLiteGenerator::GenerateMembers(
    io::Printer* printer) const {
  PrintExtraFieldInfo(variables_, printer);
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "@java.lang.Override\n"
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $has_oneof_case_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "    return ($boxed_type$) $oneof_name$_;\n"
                 "  }\n"
                 "  return $default$;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void set$capitalized_name$($type$ value) {\n"
                 "$null_check$"
                 "  $set_oneof_case_message$;\n"
                 "  $oneof_name$_ = value;\n"
                 "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void clear$capitalized_name$() {\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "    $clear_oneof_case_message$;\n"
                 "    $oneof_name$_ = null;\n"
                 "  }\n"
                 "}\n");
}

void ImmutablePrimitiveOneofFieldLiteGenerator::GenerateFieldInfo(
    io::Printer* printer, std::vector<uint16>* output) const {
  WriteIntToUtf16CharSequence(descriptor_->number(), output);
  WriteIntToUtf16CharSequence(GetExperimentalJavaFieldType(descriptor_),
                              output);
  WriteIntToUtf16CharSequence(descriptor_->containing_oneof()->index(), output);
}

void ImmutablePrimitiveOneofFieldLiteGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "@java.lang.Override\n"
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return instance.has$capitalized_name$();\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public $type$ ${$get$capitalized_name$$}$() {\n"
                 "  return instance.get$capitalized_name$();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder "
                 "${$set$capitalized_name$$}$($type$ value) {\n"
                 "  copyOnWrite();\n"
                 "  instance.set$capitalized_name$(value);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  copyOnWrite();\n"
      "  instance.clear$capitalized_name$();\n"
      "  return this;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
}

// ===================================================================

RepeatedImmutablePrimitiveFieldLiteGenerator::
    RepeatedImmutablePrimitiveFieldLiteGenerator(
        const FieldDescriptor* descriptor, int messageBitIndex,
        Context* context)
    : descriptor_(descriptor),
      context_(context),
      name_resolver_(context->GetNameResolver()) {
  SetPrimitiveVariables(descriptor, messageBitIndex, 0,
                        context->GetFieldGeneratorInfo(descriptor),
                        name_resolver_, &variables_);
}

RepeatedImmutablePrimitiveFieldLiteGenerator::
    ~RepeatedImmutablePrimitiveFieldLiteGenerator() {}

int RepeatedImmutablePrimitiveFieldLiteGenerator::GetNumBitsForMessage() const {
  return 0;
}

void RepeatedImmutablePrimitiveFieldLiteGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$java.util.List<$boxed_type$> "
                 "get$capitalized_name$List();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$int get$capitalized_name$Count();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$$type$ get$capitalized_name$(int index);\n");
}

void RepeatedImmutablePrimitiveFieldLiteGenerator::GenerateMembers(
    io::Printer* printer) const {
  printer->Print(variables_, "private $field_list_type$ $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public java.util.List<$boxed_type$>\n"
                 "    ${$get$capitalized_name$List$}$() {\n"
                 "  return $name$_;\n"  // note:  unmodifiable list
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "@java.lang.Override\n"
      "$deprecation$public int ${$get$capitalized_name$Count$}$() {\n"
      "  return $name$_.size();\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "@java.lang.Override\n"
      "$deprecation$public $type$ ${$get$capitalized_name$$}$(int index) {\n"
      "  return $repeated_get$(index);\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);

  if (!EnableExperimentalRuntimeForLite() && descriptor_->is_packed() &&
      context_->HasGeneratedMethods(descriptor_->containing_type())) {
    printer->Print(variables_,
                   "private int $name$MemoizedSerializedSize = -1;\n");
  }

  printer->Print(
      variables_,
      "private void ensure$capitalized_name$IsMutable() {\n"
      "  if (!$is_mutable$) {\n"
      "    $name$_ =\n"
      "        com.google.protobuf.GeneratedMessageLite.mutableCopy($name$_);\n"
      "   }\n"
      "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void set$capitalized_name$(\n"
                 "    int index, $type$ value) {\n"
                 "$null_check$"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  $repeated_set$(index, value);\n"
                 "}\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void add$capitalized_name$($type$ value) {\n"
                 "$null_check$"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  $repeated_add$(value);\n"
                 "}\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void addAll$capitalized_name$(\n"
                 "    java.lang.Iterable<? extends $boxed_type$> values) {\n"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  com.google.protobuf.AbstractMessageLite.addAll(\n"
                 "      values, $name$_);\n"
                 "}\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "private void clear$capitalized_name$() {\n"
                 "  $name$_ = $empty_list$;\n"
                 "}\n");
}

void RepeatedImmutablePrimitiveFieldLiteGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "@java.lang.Override\n"
                 "$deprecation$public java.util.List<$boxed_type$>\n"
                 "    ${$get$capitalized_name$List$}$() {\n"
                 "  return java.util.Collections.unmodifiableList(\n"
                 "      instance.get$capitalized_name$List());\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "@java.lang.Override\n"
      "$deprecation$public int ${$get$capitalized_name$Count$}$() {\n"
      "  return instance.get$capitalized_name$Count();\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "@java.lang.Override\n"
      "$deprecation$public $type$ ${$get$capitalized_name$$}$(int index) {\n"
      "  return instance.get$capitalized_name$(index);\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
                 "    int index, $type$ value) {\n"
                 "  copyOnWrite();\n"
                 "  instance.set$capitalized_name$(index, value);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder "
                 "${$add$capitalized_name$$}$($type$ value) {\n"
                 "  copyOnWrite();\n"
                 "  instance.add$capitalized_name$(value);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$addAll$capitalized_name$$}$(\n"
                 "    java.lang.Iterable<? extends $boxed_type$> values) {\n"
                 "  copyOnWrite();\n"
                 "  instance.addAll$capitalized_name$(values);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  copyOnWrite();\n"
      "  instance.clear$capitalized_name$();\n"
      "  return this;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void RepeatedImmutablePrimitiveFieldLiteGenerator::GenerateFieldInfo(
    io::Printer* printer, std::vector<uint16>* output) const {
  WriteIntToUtf16CharSequence(descriptor_->number(), output);
  WriteIntToUtf16CharSequence(GetExperimentalJavaFieldType(descriptor_),
                              output);
  printer->Print(variables_, "\"$name$_\",\n");
}

void RepeatedImmutablePrimitiveFieldLiteGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  printer->Print(variables_, "$name$_ = $empty_list$;\n");
}

std::string RepeatedImmutablePrimitiveFieldLiteGenerator::GetBoxedType() const {
  return BoxedPrimitiveTypeName(GetJavaType(descriptor_));
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
