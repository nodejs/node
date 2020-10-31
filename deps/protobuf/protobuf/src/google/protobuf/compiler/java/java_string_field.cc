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
// Author: jonp@google.com (Jon Perlow)
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
#include <google/protobuf/compiler/java/java_string_field.h>
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

void SetPrimitiveVariables(const FieldDescriptor* descriptor,
                           int messageBitIndex, int builderBitIndex,
                           const FieldGeneratorInfo* info,
                           ClassNameResolver* name_resolver,
                           std::map<std::string, std::string>* variables) {
  SetCommonFieldVariables(descriptor, info, variables);

  (*variables)["empty_list"] = "com.google.protobuf.LazyStringArrayList.EMPTY";

  (*variables)["default"] = ImmutableDefaultValue(descriptor, name_resolver);
  (*variables)["default_init"] =
      "= " + ImmutableDefaultValue(descriptor, name_resolver);
  (*variables)["capitalized_type"] = "String";
  (*variables)["tag"] =
      StrCat(static_cast<int32>(WireFormat::MakeTag(descriptor)));
  (*variables)["tag_size"] = StrCat(
      WireFormat::TagSize(descriptor->number(), GetType(descriptor)));
  (*variables)["null_check"] =
      "  if (value == null) {\n"
      "    throw new NullPointerException();\n"
      "  }\n";
  (*variables)["writeString"] = "com.google.protobuf.GeneratedMessage" +
                                GeneratedCodeVersionSuffix() + ".writeString";
  (*variables)["computeStringSize"] = "com.google.protobuf.GeneratedMessage" +
                                      GeneratedCodeVersionSuffix() +
                                      ".computeStringSize";

  // TODO(birdo): Add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] =
      descriptor->options().deprecated() ? "@java.lang.Deprecated " : "";
  (*variables)["on_changed"] = "onChanged();";

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
        "!get" + (*variables)["capitalized_name"] + "Bytes().isEmpty()";
  }

  // For repeated builders, one bit is used for whether the array is immutable.
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

ImmutableStringFieldGenerator::ImmutableStringFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  SetPrimitiveVariables(descriptor, messageBitIndex, builderBitIndex,
                        context->GetFieldGeneratorInfo(descriptor),
                        name_resolver_, &variables_);
}

ImmutableStringFieldGenerator::~ImmutableStringFieldGenerator() {}

int ImmutableStringFieldGenerator::GetNumBitsForMessage() const {
  return SupportFieldPresence(descriptor_->file()) ? 1 : 0;
}

int ImmutableStringFieldGenerator::GetNumBitsForBuilder() const {
  return GetNumBitsForMessage();
}

// A note about how strings are handled. This code used to just store a String
// in the Message. This had two issues:
//
//  1. It wouldn't roundtrip byte arrays that were not vaid UTF-8 encoded
//     strings, but rather fields that were raw bytes incorrectly marked
//     as strings in the proto file. This is common because in the proto1
//     syntax, string was the way to indicate bytes and C++ engineers can
//     easily make this mistake without affecting the C++ API. By converting to
//     strings immediately, some java code might corrupt these byte arrays as
//     it passes through a java server even if the field was never accessed by
//     application code.
//
//  2. There's a performance hit to converting between bytes and strings and
//     it many cases, the field is never even read by the application code. This
//     avoids unnecessary conversions in the common use cases.
//
// So now, the field for String is maintained as an Object reference which can
// either store a String or a ByteString. The code uses an instanceof check
// to see which one it has and converts to the other one if needed. It remembers
// the last value requested (in a thread safe manner) as this is most likely
// the one needed next. The thread safety is such that if two threads both
// convert the field because the changes made by each thread were not visible to
// the other, they may cause a conversion to happen more times than would
// otherwise be necessary. This was deemed better than adding synchronization
// overhead. It will not cause any corruption issues or affect the behavior of
// the API. The instanceof check is also highly optimized in the JVM and we
// decided it was better to reduce the memory overhead by not having two
// separate fields but rather use dynamic type checking.
//
// For single fields, the logic for this is done inside the generated code. For
// repeated fields, the logic is done in LazyStringArrayList and
// UnmodifiableLazyStringList.
void ImmutableStringFieldGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$boolean has$capitalized_name$();\n");
  }
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$java.lang.String get$capitalized_name$();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$com.google.protobuf.ByteString\n"
                 "    get$capitalized_name$Bytes();\n");
}

void ImmutableStringFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  printer->Print(variables_, "private volatile java.lang.Object $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);

  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $get_has_field_bit_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.lang.String ${$get$capitalized_name$$}$() {\n"
      "  java.lang.Object ref = $name$_;\n"
      "  if (ref instanceof java.lang.String) {\n"
      "    return (java.lang.String) ref;\n"
      "  } else {\n"
      "    com.google.protobuf.ByteString bs = \n"
      "        (com.google.protobuf.ByteString) ref;\n"
      "    java.lang.String s = bs.toStringUtf8();\n");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "    $name$_ = s;\n");
  } else {
    printer->Print(variables_,
                   "    if (bs.isValidUtf8()) {\n"
                   "      $name$_ = s;\n"
                   "    }\n");
  }
  printer->Print(variables_,
                 "    return s;\n"
                 "  }\n"
                 "}\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$() {\n"
                 "  java.lang.Object ref = $name$_;\n"
                 "  if (ref instanceof java.lang.String) {\n"
                 "    com.google.protobuf.ByteString b = \n"
                 "        com.google.protobuf.ByteString.copyFromUtf8(\n"
                 "            (java.lang.String) ref);\n"
                 "    $name$_ = b;\n"
                 "    return b;\n"
                 "  } else {\n"
                 "    return (com.google.protobuf.ByteString) ref;\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void ImmutableStringFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "private java.lang.Object $name$_ $default_init$;\n");
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $get_has_field_bit_builder$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.lang.String ${$get$capitalized_name$$}$() {\n"
      "  java.lang.Object ref = $name$_;\n"
      "  if (!(ref instanceof java.lang.String)) {\n"
      "    com.google.protobuf.ByteString bs =\n"
      "        (com.google.protobuf.ByteString) ref;\n"
      "    java.lang.String s = bs.toStringUtf8();\n");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "    $name$_ = s;\n");
  } else {
    printer->Print(variables_,
                   "    if (bs.isValidUtf8()) {\n"
                   "      $name$_ = s;\n"
                   "    }\n");
  }
  printer->Print(variables_,
                 "    return s;\n"
                 "  } else {\n"
                 "    return (java.lang.String) ref;\n"
                 "  }\n"
                 "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$() {\n"
                 "  java.lang.Object ref = $name$_;\n"
                 "  if (ref instanceof String) {\n"
                 "    com.google.protobuf.ByteString b = \n"
                 "        com.google.protobuf.ByteString.copyFromUtf8(\n"
                 "            (java.lang.String) ref);\n"
                 "    $name$_ = b;\n"
                 "    return b;\n"
                 "  } else {\n"
                 "    return (com.google.protobuf.ByteString) ref;\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
                 "    java.lang.String value) {\n"
                 "$null_check$"
                 "  $set_has_field_bit_builder$\n"
                 "  $name$_ = value;\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  $clear_has_field_bit_builder$\n");
  printer->Annotate("{", "}", descriptor_);
  // The default value is not a simple literal so we want to avoid executing
  // it multiple times.  Instead, get the default out of the default instance.
  printer->Print(variables_,
                 "  $name$_ = getDefaultInstance().get$capitalized_name$();\n");
  printer->Print(variables_,
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$set$capitalized_name$Bytes$}$(\n"
      "    com.google.protobuf.ByteString value) {\n"
      "$null_check$");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "  checkByteStringIsUtf8(value);\n");
  }
  printer->Print(variables_,
                 "  $set_has_field_bit_builder$\n"
                 "  $name$_ = value;\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
}

void ImmutableStringFieldGenerator::GenerateFieldBuilderInitializationCode(
    io::Printer* printer) const {
  // noop for primitives
}

void ImmutableStringFieldGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  printer->Print(variables_, "$name$_ = $default$;\n");
}

void ImmutableStringFieldGenerator::GenerateBuilderClearCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "$name$_ = $default$;\n"
                 "$clear_has_field_bit_builder$\n");
}

void ImmutableStringFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    // Allow a slight breach of abstraction here in order to avoid forcing
    // all string fields to Strings when copying fields from a Message.
    printer->Print(variables_,
                   "if (other.has$capitalized_name$()) {\n"
                   "  $set_has_field_bit_builder$\n"
                   "  $name$_ = other.$name$_;\n"
                   "  $on_changed$\n"
                   "}\n");
  } else {
    printer->Print(variables_,
                   "if (!other.get$capitalized_name$().isEmpty()) {\n"
                   "  $name$_ = other.$name$_;\n"
                   "  $on_changed$\n"
                   "}\n");
  }
}

void ImmutableStringFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    printer->Print(variables_,
                   "if ($get_has_field_bit_from_local$) {\n"
                   "  $set_has_field_bit_to_local$;\n"
                   "}\n");
  }
  printer->Print(variables_, "result.$name$_ = $name$_;\n");
}

void ImmutableStringFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_,
                   "java.lang.String s = input.readStringRequireUtf8();\n"
                   "$set_has_field_bit_message$\n"
                   "$name$_ = s;\n");
  } else {
    printer->Print(variables_,
                   "com.google.protobuf.ByteString bs = input.readBytes();\n"
                   "$set_has_field_bit_message$\n"
                   "$name$_ = bs;\n");
  }
}

void ImmutableStringFieldGenerator::GenerateParsingDoneCode(
    io::Printer* printer) const {
  // noop for strings.
}

void ImmutableStringFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($is_field_present_message$) {\n"
                 "  $writeString$(output, $number$, $name$_);\n"
                 "}\n");
}

void ImmutableStringFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($is_field_present_message$) {\n"
                 "  size += $computeStringSize$($number$, $name$_);\n"
                 "}\n");
}

void ImmutableStringFieldGenerator::GenerateEqualsCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (!get$capitalized_name$()\n"
                 "    .equals(other.get$capitalized_name$())) return false;\n");
}

void ImmutableStringFieldGenerator::GenerateHashCode(
    io::Printer* printer) const {
  printer->Print(variables_, "hash = (37 * hash) + $constant_name$;\n");
  printer->Print(variables_,
                 "hash = (53 * hash) + get$capitalized_name$().hashCode();\n");
}

std::string ImmutableStringFieldGenerator::GetBoxedType() const {
  return "java.lang.String";
}

// ===================================================================

ImmutableStringOneofFieldGenerator::ImmutableStringOneofFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : ImmutableStringFieldGenerator(descriptor, messageBitIndex,
                                    builderBitIndex, context) {
  const OneofGeneratorInfo* info =
      context->GetOneofGeneratorInfo(descriptor->containing_oneof());
  SetCommonOneofVariables(descriptor, info, &variables_);
}

ImmutableStringOneofFieldGenerator::~ImmutableStringOneofFieldGenerator() {}

void ImmutableStringOneofFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  PrintExtraFieldInfo(variables_, printer);

  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $has_oneof_case_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.lang.String ${$get$capitalized_name$$}$() {\n"
      "  java.lang.Object ref $default_init$;\n"
      "  if ($has_oneof_case_message$) {\n"
      "    ref = $oneof_name$_;\n"
      "  }\n"
      "  if (ref instanceof java.lang.String) {\n"
      "    return (java.lang.String) ref;\n"
      "  } else {\n"
      "    com.google.protobuf.ByteString bs = \n"
      "        (com.google.protobuf.ByteString) ref;\n"
      "    java.lang.String s = bs.toStringUtf8();\n");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_,
                   "    if ($has_oneof_case_message$) {\n"
                   "      $oneof_name$_ = s;\n"
                   "    }\n");
  } else {
    printer->Print(variables_,
                   "    if (bs.isValidUtf8() && ($has_oneof_case_message$)) {\n"
                   "      $oneof_name$_ = s;\n"
                   "    }\n");
  }
  printer->Print(variables_,
                 "    return s;\n"
                 "  }\n"
                 "}\n");
  WriteFieldDocComment(printer, descriptor_);

  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$() {\n"
                 "  java.lang.Object ref $default_init$;\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "    ref = $oneof_name$_;\n"
                 "  }\n"
                 "  if (ref instanceof java.lang.String) {\n"
                 "    com.google.protobuf.ByteString b = \n"
                 "        com.google.protobuf.ByteString.copyFromUtf8(\n"
                 "            (java.lang.String) ref);\n"
                 "    if ($has_oneof_case_message$) {\n"
                 "      $oneof_name$_ = b;\n"
                 "    }\n"
                 "    return b;\n"
                 "  } else {\n"
                 "    return (com.google.protobuf.ByteString) ref;\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void ImmutableStringOneofFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  if (SupportFieldPresence(descriptor_->file())) {
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public boolean ${$has$capitalized_name$$}$() {\n"
        "  return $has_oneof_case_message$;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
  }

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public java.lang.String ${$get$capitalized_name$$}$() {\n"
      "  java.lang.Object ref $default_init$;\n"
      "  if ($has_oneof_case_message$) {\n"
      "    ref = $oneof_name$_;\n"
      "  }\n"
      "  if (!(ref instanceof java.lang.String)) {\n"
      "    com.google.protobuf.ByteString bs =\n"
      "        (com.google.protobuf.ByteString) ref;\n"
      "    java.lang.String s = bs.toStringUtf8();\n"
      "    if ($has_oneof_case_message$) {\n");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "      $oneof_name$_ = s;\n");
  } else {
    printer->Print(variables_,
                   "      if (bs.isValidUtf8()) {\n"
                   "        $oneof_name$_ = s;\n"
                   "      }\n");
  }
  printer->Print(variables_,
                 "    }\n"
                 "    return s;\n"
                 "  } else {\n"
                 "    return (java.lang.String) ref;\n"
                 "  }\n"
                 "}\n");

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$() {\n"
                 "  java.lang.Object ref $default_init$;\n"
                 "  if ($has_oneof_case_message$) {\n"
                 "    ref = $oneof_name$_;\n"
                 "  }\n"
                 "  if (ref instanceof String) {\n"
                 "    com.google.protobuf.ByteString b = \n"
                 "        com.google.protobuf.ByteString.copyFromUtf8(\n"
                 "            (java.lang.String) ref);\n"
                 "    if ($has_oneof_case_message$) {\n"
                 "      $oneof_name$_ = b;\n"
                 "    }\n"
                 "    return b;\n"
                 "  } else {\n"
                 "    return (com.google.protobuf.ByteString) ref;\n"
                 "  }\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
                 "    java.lang.String value) {\n"
                 "$null_check$"
                 "  $set_oneof_case_message$;\n"
                 "  $oneof_name$_ = value;\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  if ($has_oneof_case_message$) {\n"
      "    $clear_oneof_case_message$;\n"
      "    $oneof_name$_ = null;\n"
      "    $on_changed$\n"
      "  }\n"
      "  return this;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$set$capitalized_name$Bytes$}$(\n"
      "    com.google.protobuf.ByteString value) {\n"
      "$null_check$");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "  checkByteStringIsUtf8(value);\n");
  }
  printer->Print(variables_,
                 "  $set_oneof_case_message$;\n"
                 "  $oneof_name$_ = value;\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
}

void ImmutableStringOneofFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  // Allow a slight breach of abstraction here in order to avoid forcing
  // all string fields to Strings when copying fields from a Message.
  printer->Print(variables_,
                 "$set_oneof_case_message$;\n"
                 "$oneof_name$_ = other.$oneof_name$_;\n"
                 "$on_changed$\n");
}

void ImmutableStringOneofFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($has_oneof_case_message$) {\n"
                 "  result.$oneof_name$_ = $oneof_name$_;\n"
                 "}\n");
}

void ImmutableStringOneofFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_,
                   "java.lang.String s = input.readStringRequireUtf8();\n"
                   "$set_oneof_case_message$;\n"
                   "$oneof_name$_ = s;\n");
  } else {
    printer->Print(variables_,
                   "com.google.protobuf.ByteString bs = input.readBytes();\n"
                   "$set_oneof_case_message$;\n"
                   "$oneof_name$_ = bs;\n");
  }
}

void ImmutableStringOneofFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($has_oneof_case_message$) {\n"
                 "  $writeString$(output, $number$, $oneof_name$_);\n"
                 "}\n");
}

void ImmutableStringOneofFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($has_oneof_case_message$) {\n"
                 "  size += $computeStringSize$($number$, $oneof_name$_);\n"
                 "}\n");
}

// ===================================================================

RepeatedImmutableStringFieldGenerator::RepeatedImmutableStringFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  SetPrimitiveVariables(descriptor, messageBitIndex, builderBitIndex,
                        context->GetFieldGeneratorInfo(descriptor),
                        name_resolver_, &variables_);
}

RepeatedImmutableStringFieldGenerator::
    ~RepeatedImmutableStringFieldGenerator() {}

int RepeatedImmutableStringFieldGenerator::GetNumBitsForMessage() const {
  return 0;
}

int RepeatedImmutableStringFieldGenerator::GetNumBitsForBuilder() const {
  return 1;
}

void RepeatedImmutableStringFieldGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      // NOTE: the same method in the implementation class actually returns
      // com.google.protobuf.ProtocolStringList (a subclass of List). It's
      // changed between protobuf 2.5.0 release and protobuf 2.6.1 release.
      // To retain binary compatibility with both 2.5.0 and 2.6.1 generated
      // code, we make this interface method return List so both methods
      // with different return types exist in the compiled byte code.
      "$deprecation$java.util.List<java.lang.String>\n"
      "    get$capitalized_name$List();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$int get$capitalized_name$Count();\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$java.lang.String get$capitalized_name$(int index);\n");
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$com.google.protobuf.ByteString\n"
                 "    get$capitalized_name$Bytes(int index);\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateMembers(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "private com.google.protobuf.LazyStringList $name$_;\n");
  PrintExtraFieldInfo(variables_, printer);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ProtocolStringList\n"
                 "    ${$get$capitalized_name$List$}$() {\n"
                 "  return $name$_;\n"  // note:  unmodifiable list
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
  printer->Print(variables_,
                 "$deprecation$public java.lang.String "
                 "${$get$capitalized_name$$}$(int index) {\n"
                 "  return $name$_.get(index);\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$(int index) {\n"
                 "  return $name$_.getByteString(index);\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
}

void RepeatedImmutableStringFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  // One field is the list and the bit field keeps track of whether the
  // list is immutable. If it's immutable, the invariant is that it must
  // either an instance of Collections.emptyList() or it's an ArrayList
  // wrapped in a Collections.unmodifiableList() wrapper and nobody else has
  // a refererence to the underlying ArrayList. This invariant allows us to
  // share instances of lists between protocol buffers avoiding expensive
  // memory allocations. Note, immutable is a strong guarantee here -- not
  // just that the list cannot be modified via the reference but that the
  // list can never be modified.
  printer->Print(
      variables_,
      "private com.google.protobuf.LazyStringList $name$_ = $empty_list$;\n");

  printer->Print(
      variables_,
      "private void ensure$capitalized_name$IsMutable() {\n"
      "  if (!$get_mutable_bit_builder$) {\n"
      "    $name$_ = new com.google.protobuf.LazyStringArrayList($name$_);\n"
      "    $set_mutable_bit_builder$;\n"
      "   }\n"
      "}\n");

  // Note:  We return an unmodifiable list because otherwise the caller
  //   could hold on to the returned list and modify it after the message
  //   has been built, thus mutating the message which is supposed to be
  //   immutable.
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ProtocolStringList\n"
                 "    ${$get$capitalized_name$List$}$() {\n"
                 "  return $name$_.getUnmodifiableView();\n"
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
  printer->Print(variables_,
                 "$deprecation$public java.lang.String "
                 "${$get$capitalized_name$$}$(int index) {\n"
                 "  return $name$_.get(index);\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public com.google.protobuf.ByteString\n"
                 "    ${$get$capitalized_name$Bytes$}$(int index) {\n"
                 "  return $name$_.getByteString(index);\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$set$capitalized_name$$}$(\n"
                 "    int index, java.lang.String value) {\n"
                 "$null_check$"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  $name$_.set(index, value);\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$add$capitalized_name$$}$(\n"
                 "    java.lang.String value) {\n"
                 "$null_check$"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  $name$_.add(value);\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$public Builder ${$addAll$capitalized_name$$}$(\n"
                 "    java.lang.Iterable<java.lang.String> values) {\n"
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  com.google.protobuf.AbstractMessageLite.Builder.addAll(\n"
                 "      values, $name$_);\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$clear$capitalized_name$$}$() {\n"
      "  $name$_ = $empty_list$;\n"
      "  $clear_mutable_bit_builder$;\n"
      "  $on_changed$\n"
      "  return this;\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);

  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$public Builder ${$add$capitalized_name$Bytes$}$(\n"
      "    com.google.protobuf.ByteString value) {\n"
      "$null_check$");
  printer->Annotate("{", "}", descriptor_);
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "  checkByteStringIsUtf8(value);\n");
  }
  printer->Print(variables_,
                 "  ensure$capitalized_name$IsMutable();\n"
                 "  $name$_.add(value);\n"
                 "  $on_changed$\n"
                 "  return this;\n"
                 "}\n");
}

void RepeatedImmutableStringFieldGenerator::
    GenerateFieldBuilderInitializationCode(io::Printer* printer) const {
  // noop for primitives
}

void RepeatedImmutableStringFieldGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  printer->Print(variables_, "$name$_ = $empty_list$;\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateBuilderClearCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "$name$_ = $empty_list$;\n"
                 "$clear_mutable_bit_builder$;\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  // The code below does two optimizations:
  //   1. If the other list is empty, there's nothing to do. This ensures we
  //      don't allocate a new array if we already have an immutable one.
  //   2. If the other list is non-empty and our current list is empty, we can
  //      reuse the other list which is guaranteed to be immutable.
  printer->Print(variables_,
                 "if (!other.$name$_.isEmpty()) {\n"
                 "  if ($name$_.isEmpty()) {\n"
                 "    $name$_ = other.$name$_;\n"
                 "    $clear_mutable_bit_builder$;\n"
                 "  } else {\n"
                 "    ensure$capitalized_name$IsMutable();\n"
                 "    $name$_.addAll(other.$name$_);\n"
                 "  }\n"
                 "  $on_changed$\n"
                 "}\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  // The code below ensures that the result has an immutable list. If our
  // list is immutable, we can just reuse it. If not, we make it immutable.

  printer->Print(variables_,
                 "if ($get_mutable_bit_builder$) {\n"
                 "  $name$_ = $name$_.getUnmodifiableView();\n"
                 "  $clear_mutable_bit_builder$;\n"
                 "}\n"
                 "result.$name$_ = $name$_;\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_,
                   "java.lang.String s = input.readStringRequireUtf8();\n");
  } else {
    printer->Print(variables_,
                   "com.google.protobuf.ByteString bs = input.readBytes();\n");
  }
  printer->Print(variables_,
                 "if (!$get_mutable_bit_parser$) {\n"
                 "  $name$_ = new com.google.protobuf.LazyStringArrayList();\n"
                 "  $set_mutable_bit_parser$;\n"
                 "}\n");
  if (CheckUtf8(descriptor_)) {
    printer->Print(variables_, "$name$_.add(s);\n");
  } else {
    printer->Print(variables_, "$name$_.add(bs);\n");
  }
}

void RepeatedImmutableStringFieldGenerator::GenerateParsingDoneCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if ($get_mutable_bit_parser$) {\n"
                 "  $name$_ = $name$_.getUnmodifiableView();\n"
                 "}\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "for (int i = 0; i < $name$_.size(); i++) {\n"
                 "  $writeString$(output, $number$, $name$_.getRaw(i));\n"
                 "}\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "{\n"
                 "  int dataSize = 0;\n");
  printer->Indent();

  printer->Print(variables_,
                 "for (int i = 0; i < $name$_.size(); i++) {\n"
                 "  dataSize += computeStringSizeNoTag($name$_.getRaw(i));\n"
                 "}\n");

  printer->Print("size += dataSize;\n");

  printer->Print(variables_,
                 "size += $tag_size$ * get$capitalized_name$List().size();\n");

  printer->Outdent();
  printer->Print("}\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateEqualsCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if (!get$capitalized_name$List()\n"
      "    .equals(other.get$capitalized_name$List())) return false;\n");
}

void RepeatedImmutableStringFieldGenerator::GenerateHashCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "if (get$capitalized_name$Count() > 0) {\n"
      "  hash = (37 * hash) + $constant_name$;\n"
      "  hash = (53 * hash) + get$capitalized_name$List().hashCode();\n"
      "}\n");
}

std::string RepeatedImmutableStringFieldGenerator::GetBoxedType() const {
  return "String";
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
