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

#include <google/protobuf/compiler/java/java_map_field.h>

#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {

const FieldDescriptor* KeyField(const FieldDescriptor* descriptor) {
  GOOGLE_CHECK_EQ(FieldDescriptor::TYPE_MESSAGE, descriptor->type());
  const Descriptor* message = descriptor->message_type();
  GOOGLE_CHECK(message->options().map_entry());
  return message->FindFieldByName("key");
}

const FieldDescriptor* ValueField(const FieldDescriptor* descriptor) {
  GOOGLE_CHECK_EQ(FieldDescriptor::TYPE_MESSAGE, descriptor->type());
  const Descriptor* message = descriptor->message_type();
  GOOGLE_CHECK(message->options().map_entry());
  return message->FindFieldByName("value");
}

std::string TypeName(const FieldDescriptor* field,
                     ClassNameResolver* name_resolver, bool boxed) {
  if (GetJavaType(field) == JAVATYPE_MESSAGE) {
    return name_resolver->GetImmutableClassName(field->message_type());
  } else if (GetJavaType(field) == JAVATYPE_ENUM) {
    return name_resolver->GetImmutableClassName(field->enum_type());
  } else {
    return boxed ? BoxedPrimitiveTypeName(GetJavaType(field))
                 : PrimitiveTypeName(GetJavaType(field));
  }
}

std::string WireType(const FieldDescriptor* field) {
  return "com.google.protobuf.WireFormat.FieldType." +
         std::string(FieldTypeName(field->type()));
}

void SetMessageVariables(const FieldDescriptor* descriptor, int messageBitIndex,
                         int builderBitIndex, const FieldGeneratorInfo* info,
                         Context* context,
                         std::map<std::string, std::string>* variables) {
  SetCommonFieldVariables(descriptor, info, variables);
  ClassNameResolver* name_resolver = context->GetNameResolver();

  (*variables)["type"] =
      name_resolver->GetImmutableClassName(descriptor->message_type());
  const FieldDescriptor* key = KeyField(descriptor);
  const FieldDescriptor* value = ValueField(descriptor);
  const JavaType keyJavaType = GetJavaType(key);
  const JavaType valueJavaType = GetJavaType(value);

  (*variables)["key_type"] = TypeName(key, name_resolver, false);
  std::string boxed_key_type = TypeName(key, name_resolver, true);
  (*variables)["boxed_key_type"] = boxed_key_type;
  // Used for calling the serialization function.
  (*variables)["short_key_type"] =
      boxed_key_type.substr(boxed_key_type.rfind('.') + 1);
  (*variables)["key_wire_type"] = WireType(key);
  (*variables)["key_default_value"] = DefaultValue(key, true, name_resolver);
  (*variables)["key_null_check"] =
      IsReferenceType(keyJavaType)
          ? "if (key == null) { throw new java.lang.NullPointerException(); }"
          : "";
  (*variables)["value_null_check"] =
      IsReferenceType(valueJavaType)
          ? "if (value == null) { throw new java.lang.NullPointerException(); }"
          : "";
  if (valueJavaType == JAVATYPE_ENUM) {
    // We store enums as Integers internally.
    (*variables)["value_type"] = "int";
    (*variables)["boxed_value_type"] = "java.lang.Integer";
    (*variables)["value_wire_type"] = WireType(value);
    (*variables)["value_default_value"] =
        DefaultValue(value, true, name_resolver) + ".getNumber()";

    (*variables)["value_enum_type"] = TypeName(value, name_resolver, false);

    if (SupportUnknownEnumValue(descriptor->file())) {
      // Map unknown values to a special UNRECOGNIZED value if supported.
      (*variables)["unrecognized_value"] =
          (*variables)["value_enum_type"] + ".UNRECOGNIZED";
    } else {
      // Map unknown values to the default value if we don't have UNRECOGNIZED.
      (*variables)["unrecognized_value"] =
          DefaultValue(value, true, name_resolver);
    }
  } else {
    (*variables)["value_type"] = TypeName(value, name_resolver, false);
    (*variables)["boxed_value_type"] = TypeName(value, name_resolver, true);
    (*variables)["value_wire_type"] = WireType(value);
    (*variables)["value_default_value"] =
        DefaultValue(value, true, name_resolver);
  }
  (*variables)["type_parameters"] =
      (*variables)["boxed_key_type"] + ", " + (*variables)["boxed_value_type"];
  // TODO(birdo): Add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] =
      descriptor->options().deprecated() ? "@java.lang.Deprecated " : "";
  (*variables)["on_changed"] = "onChanged();";

  // For repeated fields, one bit is used for whether the array is immutable
  // in the parsing constructor.
  (*variables)["get_mutable_bit_parser"] =
      GenerateGetBitMutableLocal(builderBitIndex);
  (*variables)["set_mutable_bit_parser"] =
      GenerateSetBitMutableLocal(builderBitIndex);

  (*variables)["default_entry"] =
      (*variables)["capitalized_name"] + "DefaultEntryHolder.defaultEntry";
  (*variables)["map_field_parameter"] = (*variables)["default_entry"];
  (*variables)["descriptor"] =
      name_resolver->GetImmutableClassName(descriptor->file()) + ".internal_" +
      UniqueFileScopeIdentifier(descriptor->message_type()) + "_descriptor, ";
  (*variables)["ver"] = GeneratedCodeVersionSuffix();
}

}  // namespace

ImmutableMapFieldGenerator::ImmutableMapFieldGenerator(
    const FieldDescriptor* descriptor, int messageBitIndex, int builderBitIndex,
    Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  SetMessageVariables(descriptor, messageBitIndex, builderBitIndex,
                      context->GetFieldGeneratorInfo(descriptor), context,
                      &variables_);
}

ImmutableMapFieldGenerator::~ImmutableMapFieldGenerator() {}

int ImmutableMapFieldGenerator::GetNumBitsForMessage() const { return 0; }

int ImmutableMapFieldGenerator::GetNumBitsForBuilder() const { return 1; }

void ImmutableMapFieldGenerator::GenerateInterfaceMembers(
    io::Printer* printer) const {
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$int ${$get$capitalized_name$Count$}$();\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$boolean ${$contains$capitalized_name$$}$(\n"
                 "    $key_type$ key);\n");
  printer->Annotate("{", "}", descriptor_);
  if (GetJavaType(ValueField(descriptor_)) == JAVATYPE_ENUM) {
    printer->Print(variables_,
                   "/**\n"
                   " * Use {@link #get$capitalized_name$Map()} instead.\n"
                   " */\n"
                   "@java.lang.Deprecated\n"
                   "java.util.Map<$boxed_key_type$, $value_enum_type$>\n"
                   "${$get$capitalized_name$$}$();\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$java.util.Map<$boxed_key_type$, $value_enum_type$>\n"
        "${$get$capitalized_name$Map$}$();\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$$value_enum_type$ ${$get$capitalized_name$OrDefault$}$(\n"
        "    $key_type$ key,\n"
        "    $value_enum_type$ defaultValue);\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$$value_enum_type$ ${$get$capitalized_name$OrThrow$}$(\n"
        "    $key_type$ key);\n");
    printer->Annotate("{", "}", descriptor_);
    if (SupportUnknownEnumValue(descriptor_->file())) {
      printer->Print(
          variables_,
          "/**\n"
          " * Use {@link #get$capitalized_name$ValueMap()} instead.\n"
          " */\n"
          "@java.lang.Deprecated\n"
          "java.util.Map<$type_parameters$>\n"
          "${$get$capitalized_name$Value$}$();\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(variables_,
                     "$deprecation$java.util.Map<$type_parameters$>\n"
                     "${$get$capitalized_name$ValueMap$}$();\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(variables_,
                     "$deprecation$\n"
                     "$value_type$ ${$get$capitalized_name$ValueOrDefault$}$(\n"
                     "    $key_type$ key,\n"
                     "    $value_type$ defaultValue);\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(variables_,
                     "$deprecation$\n"
                     "$value_type$ ${$get$capitalized_name$ValueOrThrow$}$(\n"
                     "    $key_type$ key);\n");
      printer->Annotate("{", "}", descriptor_);
    }
  } else {
    printer->Print(variables_,
                   "/**\n"
                   " * Use {@link #get$capitalized_name$Map()} instead.\n"
                   " */\n"
                   "@java.lang.Deprecated\n"
                   "java.util.Map<$type_parameters$>\n"
                   "${$get$capitalized_name$$}$();\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$java.util.Map<$type_parameters$>\n"
                   "${$get$capitalized_name$Map$}$();\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "$value_type$ ${$get$capitalized_name$OrDefault$}$(\n"
                   "    $key_type$ key,\n"
                   "    $value_type$ defaultValue);\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "$value_type$ ${$get$capitalized_name$OrThrow$}$(\n"
                   "    $key_type$ key);\n");
    printer->Annotate("{", "}", descriptor_);
  }
}

void ImmutableMapFieldGenerator::GenerateMembers(io::Printer* printer) const {
  printer->Print(
      variables_,
      "private static final class $capitalized_name$DefaultEntryHolder {\n"
      "  static final com.google.protobuf.MapEntry<\n"
      "      $type_parameters$> defaultEntry =\n"
      "          com.google.protobuf.MapEntry\n"
      "          .<$type_parameters$>newDefaultInstance(\n"
      "              $descriptor$\n"
      "              $key_wire_type$,\n"
      "              $key_default_value$,\n"
      "              $value_wire_type$,\n"
      "              $value_default_value$);\n"
      "}\n");
  printer->Print(variables_,
                 "private com.google.protobuf.MapField<\n"
                 "    $type_parameters$> $name$_;\n"
                 "private com.google.protobuf.MapField<$type_parameters$>\n"
                 "internalGet$capitalized_name$() {\n"
                 "  if ($name$_ == null) {\n"
                 "    return com.google.protobuf.MapField.emptyMapField(\n"
                 "        $map_field_parameter$);\n"
                 "  }\n"
                 "  return $name$_;\n"
                 "}\n");
  if (GetJavaType(ValueField(descriptor_)) == JAVATYPE_ENUM) {
    printer->Print(
        variables_,
        "private static final\n"
        "com.google.protobuf.Internal.MapAdapter.Converter<\n"
        "    java.lang.Integer, $value_enum_type$> $name$ValueConverter =\n"
        "        com.google.protobuf.Internal.MapAdapter.newEnumConverter(\n"
        "            $value_enum_type$.internalGetValueMap(),\n"
        "            $unrecognized_value$);\n");
    printer->Print(
        variables_,
        "private static final java.util.Map<$boxed_key_type$, "
        "$value_enum_type$>\n"
        "internalGetAdapted$capitalized_name$Map(\n"
        "    java.util.Map<$boxed_key_type$, $boxed_value_type$> map) {\n"
        "  return new com.google.protobuf.Internal.MapAdapter<\n"
        "      $boxed_key_type$, $value_enum_type$, java.lang.Integer>(\n"
        "          map, $name$ValueConverter);\n"
        "}\n");
  }
  GenerateMapGetters(printer);
}

void ImmutableMapFieldGenerator::GenerateBuilderMembers(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "private com.google.protobuf.MapField<\n"
                 "    $type_parameters$> $name$_;\n"
                 "private com.google.protobuf.MapField<$type_parameters$>\n"
                 "internalGet$capitalized_name$() {\n"
                 "  if ($name$_ == null) {\n"
                 "    return com.google.protobuf.MapField.emptyMapField(\n"
                 "        $map_field_parameter$);\n"
                 "  }\n"
                 "  return $name$_;\n"
                 "}\n"
                 "private com.google.protobuf.MapField<$type_parameters$>\n"
                 "internalGetMutable$capitalized_name$() {\n"
                 "  $on_changed$;\n"
                 "  if ($name$_ == null) {\n"
                 "    $name$_ = com.google.protobuf.MapField.newMapField(\n"
                 "        $map_field_parameter$);\n"
                 "  }\n"
                 "  if (!$name$_.isMutable()) {\n"
                 "    $name$_ = $name$_.copy();\n"
                 "  }\n"
                 "  return $name$_;\n"
                 "}\n");
  GenerateMapGetters(printer);
  printer->Print(variables_,
                 "$deprecation$\n"
                 "public Builder ${$clear$capitalized_name$$}$() {\n"
                 "  internalGetMutable$capitalized_name$().getMutableMap()\n"
                 "      .clear();\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(variables_,
                 "$deprecation$\n"
                 "public Builder ${$remove$capitalized_name$$}$(\n"
                 "    $key_type$ key) {\n"
                 "  $key_null_check$\n"
                 "  internalGetMutable$capitalized_name$().getMutableMap()\n"
                 "      .remove(key);\n"
                 "  return this;\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  if (GetJavaType(ValueField(descriptor_)) == JAVATYPE_ENUM) {
    printer->Print(
        variables_,
        "/**\n"
        " * Use alternate mutation accessors instead.\n"
        " */\n"
        "@java.lang.Deprecated\n"
        "public java.util.Map<$boxed_key_type$, $value_enum_type$>\n"
        "${$getMutable$capitalized_name$$}$() {\n"
        "  return internalGetAdapted$capitalized_name$Map(\n"
        "       internalGetMutable$capitalized_name$().getMutableMap());\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$public Builder ${$put$capitalized_name$$}$(\n"
                   "    $key_type$ key,\n"
                   "    $value_enum_type$ value) {\n"
                   "  $key_null_check$\n"
                   "  $value_null_check$\n"
                   "  internalGetMutable$capitalized_name$().getMutableMap()\n"
                   "      .put(key, $name$ValueConverter.doBackward(value));\n"
                   "  return this;\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$public Builder ${$putAll$capitalized_name$$}$(\n"
        "    java.util.Map<$boxed_key_type$, $value_enum_type$> values) {\n"
        "  internalGetAdapted$capitalized_name$Map(\n"
        "      internalGetMutable$capitalized_name$().getMutableMap())\n"
        "          .putAll(values);\n"
        "  return this;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    if (SupportUnknownEnumValue(descriptor_->file())) {
      printer->Print(
          variables_,
          "/**\n"
          " * Use alternate mutation accessors instead.\n"
          " */\n"
          "@java.lang.Deprecated\n"
          "public java.util.Map<$boxed_key_type$, $boxed_value_type$>\n"
          "${$getMutable$capitalized_name$Value$}$() {\n"
          "  return internalGetMutable$capitalized_name$().getMutableMap();\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(
          variables_,
          "$deprecation$public Builder ${$put$capitalized_name$Value$}$(\n"
          "    $key_type$ key,\n"
          "    $value_type$ value) {\n"
          "  $key_null_check$\n"
          "  internalGetMutable$capitalized_name$().getMutableMap()\n"
          "      .put(key, value);\n"
          "  return this;\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(
          variables_,
          "$deprecation$public Builder ${$putAll$capitalized_name$Value$}$(\n"
          "    java.util.Map<$boxed_key_type$, $boxed_value_type$> values) {\n"
          "  internalGetMutable$capitalized_name$().getMutableMap()\n"
          "      .putAll(values);\n"
          "  return this;\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
    }
  } else {
    printer->Print(
        variables_,
        "/**\n"
        " * Use alternate mutation accessors instead.\n"
        " */\n"
        "@java.lang.Deprecated\n"
        "public java.util.Map<$type_parameters$>\n"
        "${$getMutable$capitalized_name$$}$() {\n"
        "  return internalGetMutable$capitalized_name$().getMutableMap();\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$"
                   "public Builder ${$put$capitalized_name$$}$(\n"
                   "    $key_type$ key,\n"
                   "    $value_type$ value) {\n"
                   "  $key_null_check$\n"
                   "  $value_null_check$\n"
                   "  internalGetMutable$capitalized_name$().getMutableMap()\n"
                   "      .put(key, value);\n"
                   "  return this;\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "public Builder ${$putAll$capitalized_name$$}$(\n"
                   "    java.util.Map<$type_parameters$> values) {\n"
                   "  internalGetMutable$capitalized_name$().getMutableMap()\n"
                   "      .putAll(values);\n"
                   "  return this;\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
  }
}

void ImmutableMapFieldGenerator::GenerateMapGetters(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "$deprecation$\n"
                 "public int ${$get$capitalized_name$Count$}$() {\n"
                 "  return internalGet$capitalized_name$().getMap().size();\n"
                 "}\n");
  printer->Annotate("{", "}", descriptor_);
  WriteFieldDocComment(printer, descriptor_);
  printer->Print(
      variables_,
      "$deprecation$\n"
      "public boolean ${$contains$capitalized_name$$}$(\n"
      "    $key_type$ key) {\n"
      "  $key_null_check$\n"
      "  return internalGet$capitalized_name$().getMap().containsKey(key);\n"
      "}\n");
  printer->Annotate("{", "}", descriptor_);
  if (GetJavaType(ValueField(descriptor_)) == JAVATYPE_ENUM) {
    printer->Print(variables_,
                   "/**\n"
                   " * Use {@link #get$capitalized_name$Map()} instead.\n"
                   " */\n"
                   "@java.lang.Deprecated\n"
                   "public java.util.Map<$boxed_key_type$, $value_enum_type$>\n"
                   "${$get$capitalized_name$$}$() {\n"
                   "  return get$capitalized_name$Map();\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "public java.util.Map<$boxed_key_type$, $value_enum_type$>\n"
                   "${$get$capitalized_name$Map$}$() {\n"
                   "  return internalGetAdapted$capitalized_name$Map(\n"
                   "      internalGet$capitalized_name$().getMap());"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$\n"
        "public $value_enum_type$ ${$get$capitalized_name$OrDefault$}$(\n"
        "    $key_type$ key,\n"
        "    $value_enum_type$ defaultValue) {\n"
        "  $key_null_check$\n"
        "  java.util.Map<$boxed_key_type$, $boxed_value_type$> map =\n"
        "      internalGet$capitalized_name$().getMap();\n"
        "  return map.containsKey(key)\n"
        "         ? $name$ValueConverter.doForward(map.get(key))\n"
        "         : defaultValue;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$\n"
        "public $value_enum_type$ ${$get$capitalized_name$OrThrow$}$(\n"
        "    $key_type$ key) {\n"
        "  $key_null_check$\n"
        "  java.util.Map<$boxed_key_type$, $boxed_value_type$> map =\n"
        "      internalGet$capitalized_name$().getMap();\n"
        "  if (!map.containsKey(key)) {\n"
        "    throw new java.lang.IllegalArgumentException();\n"
        "  }\n"
        "  return $name$ValueConverter.doForward(map.get(key));\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    if (SupportUnknownEnumValue(descriptor_->file())) {
      printer->Print(
          variables_,
          "/**\n"
          " * Use {@link #get$capitalized_name$ValueMap()} instead.\n"
          " */\n"
          "@java.lang.Deprecated\n"
          "public java.util.Map<$boxed_key_type$, $boxed_value_type$>\n"
          "${$get$capitalized_name$Value$}$() {\n"
          "  return get$capitalized_name$ValueMap();\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(
          variables_,
          "$deprecation$\n"
          "public java.util.Map<$boxed_key_type$, $boxed_value_type$>\n"
          "${$get$capitalized_name$ValueMap$}$() {\n"
          "  return internalGet$capitalized_name$().getMap();\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(
          variables_,
          "$deprecation$\n"
          "public $value_type$ ${$get$capitalized_name$ValueOrDefault$}$(\n"
          "    $key_type$ key,\n"
          "    $value_type$ defaultValue) {\n"
          "  $key_null_check$\n"
          "  java.util.Map<$boxed_key_type$, $boxed_value_type$> map =\n"
          "      internalGet$capitalized_name$().getMap();\n"
          "  return map.containsKey(key) ? map.get(key) : defaultValue;\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
      WriteFieldDocComment(printer, descriptor_);
      printer->Print(
          variables_,
          "$deprecation$\n"
          "public $value_type$ ${$get$capitalized_name$ValueOrThrow$}$(\n"
          "    $key_type$ key) {\n"
          "  $key_null_check$\n"
          "  java.util.Map<$boxed_key_type$, $boxed_value_type$> map =\n"
          "      internalGet$capitalized_name$().getMap();\n"
          "  if (!map.containsKey(key)) {\n"
          "    throw new java.lang.IllegalArgumentException();\n"
          "  }\n"
          "  return map.get(key);\n"
          "}\n");
      printer->Annotate("{", "}", descriptor_);
    }
  } else {
    printer->Print(variables_,
                   "/**\n"
                   " * Use {@link #get$capitalized_name$Map()} instead.\n"
                   " */\n"
                   "@java.lang.Deprecated\n"
                   "public java.util.Map<$type_parameters$> "
                   "${$get$capitalized_name$$}$() {\n"
                   "  return get$capitalized_name$Map();\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "public java.util.Map<$type_parameters$> "
                   "${$get$capitalized_name$Map$}$() {\n"
                   "  return internalGet$capitalized_name$().getMap();\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(
        variables_,
        "$deprecation$\n"
        "public $value_type$ ${$get$capitalized_name$OrDefault$}$(\n"
        "    $key_type$ key,\n"
        "    $value_type$ defaultValue) {\n"
        "  $key_null_check$\n"
        "  java.util.Map<$type_parameters$> map =\n"
        "      internalGet$capitalized_name$().getMap();\n"
        "  return map.containsKey(key) ? map.get(key) : defaultValue;\n"
        "}\n");
    printer->Annotate("{", "}", descriptor_);
    WriteFieldDocComment(printer, descriptor_);
    printer->Print(variables_,
                   "$deprecation$\n"
                   "public $value_type$ ${$get$capitalized_name$OrThrow$}$(\n"
                   "    $key_type$ key) {\n"
                   "  $key_null_check$\n"
                   "  java.util.Map<$type_parameters$> map =\n"
                   "      internalGet$capitalized_name$().getMap();\n"
                   "  if (!map.containsKey(key)) {\n"
                   "    throw new java.lang.IllegalArgumentException();\n"
                   "  }\n"
                   "  return map.get(key);\n"
                   "}\n");
    printer->Annotate("{", "}", descriptor_);
  }
}

void ImmutableMapFieldGenerator::GenerateFieldBuilderInitializationCode(
    io::Printer* printer) const {
  // Nothing to initialize.
}

void ImmutableMapFieldGenerator::GenerateInitializationCode(
    io::Printer* printer) const {
  // Nothing to initialize.
}

void ImmutableMapFieldGenerator::GenerateBuilderClearCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "internalGetMutable$capitalized_name$().clear();\n");
}

void ImmutableMapFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "internalGetMutable$capitalized_name$().mergeFrom(\n"
                 "    other.internalGet$capitalized_name$());\n");
}

void ImmutableMapFieldGenerator::GenerateBuildingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "result.$name$_ = internalGet$capitalized_name$();\n"
                 "result.$name$_.makeImmutable();\n");
}

void ImmutableMapFieldGenerator::GenerateParsingCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (!$get_mutable_bit_parser$) {\n"
                 "  $name$_ = com.google.protobuf.MapField.newMapField(\n"
                 "      $map_field_parameter$);\n"
                 "  $set_mutable_bit_parser$;\n"
                 "}\n");
  if (!SupportUnknownEnumValue(descriptor_->file()) &&
      GetJavaType(ValueField(descriptor_)) == JAVATYPE_ENUM) {
    printer->Print(
        variables_,
        "com.google.protobuf.ByteString bytes = input.readBytes();\n"
        "com.google.protobuf.MapEntry<$type_parameters$>\n"
        "$name$__ = $default_entry$.getParserForType().parseFrom(bytes);\n");
    printer->Print(
        variables_,
        "if ($value_enum_type$.forNumber($name$__.getValue()) == null) {\n"
        "  unknownFields.mergeLengthDelimitedField($number$, bytes);\n"
        "} else {\n"
        "  $name$_.getMutableMap().put(\n"
        "      $name$__.getKey(), $name$__.getValue());\n"
        "}\n");
  } else {
    printer->Print(
        variables_,
        "com.google.protobuf.MapEntry<$type_parameters$>\n"
        "$name$__ = input.readMessage(\n"
        "    $default_entry$.getParserForType(), extensionRegistry);\n"
        "$name$_.getMutableMap().put(\n"
        "    $name$__.getKey(), $name$__.getValue());\n");
  }
}

void ImmutableMapFieldGenerator::GenerateParsingDoneCode(
    io::Printer* printer) const {
  // Nothing to do here.
}

void ImmutableMapFieldGenerator::GenerateSerializationCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "com.google.protobuf.GeneratedMessage$ver$\n"
                 "  .serialize$short_key_type$MapTo(\n"
                 "    output,\n"
                 "    internalGet$capitalized_name$(),\n"
                 "    $default_entry$,\n"
                 "    $number$);\n");
}

void ImmutableMapFieldGenerator::GenerateSerializedSizeCode(
    io::Printer* printer) const {
  printer->Print(
      variables_,
      "for (java.util.Map.Entry<$type_parameters$> entry\n"
      "     : internalGet$capitalized_name$().getMap().entrySet()) {\n"
      "  com.google.protobuf.MapEntry<$type_parameters$>\n"
      "  $name$__ = $default_entry$.newBuilderForType()\n"
      "      .setKey(entry.getKey())\n"
      "      .setValue(entry.getValue())\n"
      "      .build();\n"
      "  size += com.google.protobuf.CodedOutputStream\n"
      "      .computeMessageSize($number$, $name$__);\n"
      "}\n");
}

void ImmutableMapFieldGenerator::GenerateEqualsCode(
    io::Printer* printer) const {
  printer->Print(variables_,
                 "if (!internalGet$capitalized_name$().equals(\n"
                 "    other.internalGet$capitalized_name$())) return false;\n");
}

void ImmutableMapFieldGenerator::GenerateHashCode(io::Printer* printer) const {
  printer->Print(
      variables_,
      "if (!internalGet$capitalized_name$().getMap().isEmpty()) {\n"
      "  hash = (37 * hash) + $constant_name$;\n"
      "  hash = (53 * hash) + internalGet$capitalized_name$().hashCode();\n"
      "}\n");
}

std::string ImmutableMapFieldGenerator::GetBoxedType() const {
  return name_resolver_->GetImmutableClassName(descriptor_->message_type());
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
