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

// Author: dweis@google.com (Daniel Weis)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/java/java_message_lite.h>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_enum_lite.h>
#include <google/protobuf/compiler/java/java_extension_lite.h>
#include <google/protobuf/compiler/java/java_generator_factory.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_message_builder.h>
#include <google/protobuf/compiler/java/java_message_builder_lite.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>



namespace google {
namespace protobuf {
namespace compiler {
namespace java {

using internal::WireFormat;
using internal::WireFormatLite;

// ===================================================================
ImmutableMessageLiteGenerator::ImmutableMessageLiteGenerator(
    const Descriptor* descriptor, Context* context)
    : MessageGenerator(descriptor),
      context_(context),
      name_resolver_(context->GetNameResolver()),
      field_generators_(descriptor, context_) {
  GOOGLE_CHECK(!HasDescriptorMethods(descriptor->file(), context->EnforceLite()))
      << "Generator factory error: A lite message generator is used to "
         "generate non-lite messages.";
}

ImmutableMessageLiteGenerator::~ImmutableMessageLiteGenerator() {}

void ImmutableMessageLiteGenerator::GenerateStaticVariables(
    io::Printer* printer, int* bytecode_estimate) {
  // Generate static members for all nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    // TODO(kenton):  Reuse MessageGenerator objects?
    ImmutableMessageLiteGenerator(descriptor_->nested_type(i), context_)
        .GenerateStaticVariables(printer, bytecode_estimate);
  }
}

int ImmutableMessageLiteGenerator::GenerateStaticVariableInitializers(
    io::Printer* printer) {
  int bytecode_estimate = 0;
  // Generate static member initializers for all nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    // TODO(kenton):  Reuse MessageGenerator objects?
    bytecode_estimate +=
        ImmutableMessageLiteGenerator(descriptor_->nested_type(i), context_)
            .GenerateStaticVariableInitializers(printer);
  }
  return bytecode_estimate;
}

// ===================================================================

void ImmutableMessageLiteGenerator::GenerateInterface(io::Printer* printer) {
  MaybePrintGeneratedAnnotation(context_, printer, descriptor_,
                                /* immutable = */ true, "OrBuilder");
  if (descriptor_->extension_range_count() > 0) {
    printer->Print(
        "$deprecation$public interface ${$$classname$OrBuilder$}$ extends \n"
        "    $extra_interfaces$\n"
        "     com.google.protobuf.GeneratedMessageLite.\n"
        "          ExtendableMessageOrBuilder<\n"
        "              $classname$, $classname$.Builder> {\n",
        "deprecation",
        descriptor_->options().deprecated() ? "@java.lang.Deprecated " : "",
        "extra_interfaces", ExtraMessageOrBuilderInterfaces(descriptor_),
        "classname", descriptor_->name(), "{", "", "}", "");
  } else {
    printer->Print(
        "$deprecation$public interface ${$$classname$OrBuilder$}$ extends\n"
        "    $extra_interfaces$\n"
        "    com.google.protobuf.MessageLiteOrBuilder {\n",
        "deprecation",
        descriptor_->options().deprecated() ? "@java.lang.Deprecated " : "",
        "extra_interfaces", ExtraMessageOrBuilderInterfaces(descriptor_),
        "classname", descriptor_->name(), "{", "", "}", "");
  }
  printer->Annotate("{", "}", descriptor_);

  printer->Indent();
  for (int i = 0; i < descriptor_->field_count(); i++) {
    printer->Print("\n");
    field_generators_.get(descriptor_->field(i))
        .GenerateInterfaceMembers(printer);
  }
  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    printer->Print(
        "\n"
        "public $classname$.$oneof_capitalized_name$Case "
        "get$oneof_capitalized_name$Case();\n",
        "oneof_capitalized_name",
        context_->GetOneofGeneratorInfo(descriptor_->oneof_decl(i))
            ->capitalized_name,
        "classname",
        context_->GetNameResolver()->GetImmutableClassName(descriptor_));
  }
  printer->Outdent();

  printer->Print("}\n");
}

// ===================================================================

void ImmutableMessageLiteGenerator::Generate(io::Printer* printer) {
  bool is_own_file = IsOwnFile(descriptor_, /* immutable = */ true);

  std::map<std::string, std::string> variables;
  variables["static"] = is_own_file ? " " : " static ";
  variables["classname"] = descriptor_->name();
  variables["extra_interfaces"] = ExtraMessageInterfaces(descriptor_);
  variables["deprecation"] =
      descriptor_->options().deprecated() ? "@java.lang.Deprecated " : "";

  WriteMessageDocComment(printer, descriptor_);
  MaybePrintGeneratedAnnotation(context_, printer, descriptor_,
                                /* immutable = */ true);


  // The builder_type stores the super type name of the nested Builder class.
  std::string builder_type;
  if (descriptor_->extension_range_count() > 0) {
    printer->Print(
        variables,
        "$deprecation$public $static$final class $classname$ extends\n"
        "    com.google.protobuf.GeneratedMessageLite.ExtendableMessage<\n"
        "      $classname$, $classname$.Builder> implements\n"
        "    $extra_interfaces$\n"
        "    $classname$OrBuilder {\n");
    builder_type = strings::Substitute(
        "com.google.protobuf.GeneratedMessageLite.ExtendableBuilder<$0, ?>",
        name_resolver_->GetImmutableClassName(descriptor_));
  } else {
    printer->Print(
        variables,
        "$deprecation$public $static$final class $classname$ extends\n"
        "    com.google.protobuf.GeneratedMessageLite<\n"
        "        $classname$, $classname$.Builder> implements\n"
        "    $extra_interfaces$\n"
        "    $classname$OrBuilder {\n");

    builder_type = "com.google.protobuf.GeneratedMessageLite.Builder";
  }
  printer->Indent();

  GenerateConstructor(printer);

  // Nested types
  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    EnumLiteGenerator(descriptor_->enum_type(i), true, context_)
        .Generate(printer);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    // Don't generate Java classes for map entry messages.
    if (IsMapEntry(descriptor_->nested_type(i))) continue;
    ImmutableMessageLiteGenerator messageGenerator(descriptor_->nested_type(i),
                                                   context_);
    messageGenerator.GenerateInterface(printer);
    messageGenerator.Generate(printer);
  }

  // Integers for bit fields.
  int totalBits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    totalBits +=
        field_generators_.get(descriptor_->field(i)).GetNumBitsForMessage();
  }
  int totalInts = (totalBits + 31) / 32;
  for (int i = 0; i < totalInts; i++) {
    printer->Print("private int $bit_field_name$;\n", "bit_field_name",
                   GetBitFieldName(i));
  }

  // oneof
  std::map<std::string, std::string> vars;
  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    const OneofDescriptor* oneof = descriptor_->oneof_decl(i);
    vars["oneof_name"] = context_->GetOneofGeneratorInfo(oneof)->name;
    vars["oneof_capitalized_name"] =
        context_->GetOneofGeneratorInfo(oneof)->capitalized_name;
    vars["oneof_index"] = StrCat(oneof->index());
    // oneofCase_ and oneof_
    printer->Print(vars,
                   "private int $oneof_name$Case_ = 0;\n"
                   "private java.lang.Object $oneof_name$_;\n");
    // OneofCase enum
    printer->Print(vars,
                   "public enum $oneof_capitalized_name$Case\n"
                   "    implements com.google.protobuf.Internal.EnumLite {\n");
    printer->Indent();
    for (int j = 0; j < oneof->field_count(); j++) {
      const FieldDescriptor* field = oneof->field(j);
      printer->Print("$field_name$($field_number$),\n", "field_name",
                     ToUpper(field->name()), "field_number",
                     StrCat(field->number()));
    }
    printer->Print("$cap_oneof_name$_NOT_SET(0);\n", "cap_oneof_name",
                   ToUpper(vars["oneof_name"]));
    printer->Print(vars,
                   "private final int value;\n"
                   "private $oneof_capitalized_name$Case(int value) {\n"
                   "  this.value = value;\n"
                   "}\n");
    printer->Print(
        vars,
        "/**\n"
        " * @deprecated Use {@link #forNumber(int)} instead.\n"
        " */\n"
        "@java.lang.Deprecated\n"
        "public static $oneof_capitalized_name$Case valueOf(int value) {\n"
        "  return forNumber(value);\n"
        "}\n"
        "\n"
        "public static $oneof_capitalized_name$Case forNumber(int value) {\n"
        "  switch (value) {\n");
    for (int j = 0; j < oneof->field_count(); j++) {
      const FieldDescriptor* field = oneof->field(j);
      printer->Print("    case $field_number$: return $field_name$;\n",
                     "field_number", StrCat(field->number()),
                     "field_name", ToUpper(field->name()));
    }
    printer->Print(
        "    case 0: return $cap_oneof_name$_NOT_SET;\n"
        "    default: return null;\n"
        "  }\n"
        "}\n"
        "@java.lang.Override\n"
        "public int getNumber() {\n"
        "  return this.value;\n"
        "}\n",
        "cap_oneof_name", ToUpper(vars["oneof_name"]));
    printer->Outdent();
    printer->Print("};\n\n");
    // oneofCase()
    printer->Print(vars,
                   "@java.lang.Override\n"
                   "public $oneof_capitalized_name$Case\n"
                   "get$oneof_capitalized_name$Case() {\n"
                   "  return $oneof_capitalized_name$Case.forNumber(\n"
                   "      $oneof_name$Case_);\n"
                   "}\n"
                   "\n"
                   "private void clear$oneof_capitalized_name$() {\n"
                   "  $oneof_name$Case_ = 0;\n"
                   "  $oneof_name$_ = null;\n"
                   "}\n"
                   "\n");
  }

  // Fields
  for (int i = 0; i < descriptor_->field_count(); i++) {
    printer->Print("public static final int $constant_name$ = $number$;\n",
                   "constant_name", FieldConstantName(descriptor_->field(i)),
                   "number", StrCat(descriptor_->field(i)->number()));
    field_generators_.get(descriptor_->field(i)).GenerateMembers(printer);
    printer->Print("\n");
  }

  GenerateParseFromMethods(printer);
  GenerateBuilder(printer);

  if (HasRequiredFields(descriptor_)) {
    // Memoizes whether the protocol buffer is fully initialized (has all
    // required fields). 0 means false, 1 means true, and all other values
    // mean not yet computed.
    printer->Print("private byte memoizedIsInitialized = 2;\n");
  }

  printer->Print(
      "@java.lang.Override\n"
      "@java.lang.SuppressWarnings({\"unchecked\", \"fallthrough\"})\n"
      "protected final java.lang.Object dynamicMethod(\n"
      "    com.google.protobuf.GeneratedMessageLite.MethodToInvoke method,\n"
      "    java.lang.Object arg0, java.lang.Object arg1) {\n"
      "  switch (method) {\n"
      "    case NEW_MUTABLE_INSTANCE: {\n"
      "      return new $classname$();\n"
      "    }\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  printer->Indent();
  printer->Indent();

  printer->Print("case NEW_BUILDER: {\n");

  printer->Indent();
  GenerateDynamicMethodNewBuilder(printer);
  printer->Outdent();

  printer->Print(
      "}\n"
      "case BUILD_MESSAGE_INFO: {\n");

  printer->Indent();
  GenerateDynamicMethodNewBuildMessageInfo(printer);
  printer->Outdent();

  printer->Print(
      "}\n"
      "// fall through\n"
      "case GET_DEFAULT_INSTANCE: {\n"
      "  return DEFAULT_INSTANCE;\n"
      "}\n"
      "case GET_PARSER: {\n"
      // Generally one would use the lazy initialization holder pattern for
      // manipulating static fields but that has exceptional cost on Android as
      // it will generate an extra class for every message. Instead, use the
      // double-check locking pattern which works just as well.
      //
      // The "parser" temporary mirrors the "PARSER" field to eliminate a read
      // at the final return statement.
      "  com.google.protobuf.Parser<$classname$> parser = PARSER;\n"
      "  if (parser == null) {\n"
      "    synchronized ($classname$.class) {\n"
      "      parser = PARSER;\n"
      "      if (parser == null) {\n"
      "        parser =\n"
      "            new DefaultInstanceBasedParser<$classname$>(\n"
      "                DEFAULT_INSTANCE);\n"
      "        PARSER = parser;\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "  return parser;\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  printer->Outdent();

  if (HasRequiredFields(descriptor_)) {
    printer->Print(
        "}\n"
        "case GET_MEMOIZED_IS_INITIALIZED: {\n"
        "  return memoizedIsInitialized;\n"
        "}\n"
        "case SET_MEMOIZED_IS_INITIALIZED: {\n"
        "  memoizedIsInitialized = (byte) (arg0 == null ? 0 : 1);\n"
        "  return null;\n"
        "}\n");
  } else {
    printer->Print(
        "}\n"
        "case GET_MEMOIZED_IS_INITIALIZED: {\n"
        "  return (byte) 1;\n"
        "}\n"
        "case SET_MEMOIZED_IS_INITIALIZED: {\n"
        "  return null;\n"
        "}\n");
  }

  printer->Outdent();
  printer->Print(
      "  }\n"
      "  throw new UnsupportedOperationException();\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  printer->Print(
      "\n"
      "// @@protoc_insertion_point(class_scope:$full_name$)\n",
      "full_name", descriptor_->full_name());

  // Carefully initialize the default instance in such a way that it doesn't
  // conflict with other initialization.
  printer->Print("private static final $classname$ DEFAULT_INSTANCE;\n",
                 "classname",
                 name_resolver_->GetImmutableClassName(descriptor_));

  printer->Print(
      "static {\n"
      "  $classname$ defaultInstance = new $classname$();\n"
      "  // New instances are implicitly immutable so no need to make\n"
      "  // immutable.\n"
      "  DEFAULT_INSTANCE = defaultInstance;\n"
      // Register the default instance in a map. This map will be used by
      // experimental runtime to lookup default instance given a class instance
      // without using Java reflection.
      "  com.google.protobuf.GeneratedMessageLite.registerDefaultInstance(\n"
      "    $classname$.class, defaultInstance);\n"
      "}\n"
      "\n",
      "classname", descriptor_->name());

  printer->Print(
      "public static $classname$ getDefaultInstance() {\n"
      "  return DEFAULT_INSTANCE;\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  // 'of' method for Wrappers
  if (IsWrappersProtoFile(descriptor_->file())) {
    printer->Print(
        "public static $classname$ of($field_type$ value) {\n"
        "  return newBuilder().setValue(value).build();\n"
        "}\n"
        "\n",
        "classname", name_resolver_->GetImmutableClassName(descriptor_),
        "field_type", PrimitiveTypeName(GetJavaType(descriptor_->field(0))));
  }

  GenerateParser(printer);

  // Extensions must be declared after the DEFAULT_INSTANCE is initialized
  // because the DEFAULT_INSTANCE is used by the extension to lazily retrieve
  // the outer class's FileDescriptor.
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    ImmutableExtensionLiteGenerator(descriptor_->extension(i), context_)
        .Generate(printer);
  }

  printer->Outdent();
  printer->Print("}\n\n");
}

void ImmutableMessageLiteGenerator::GenerateDynamicMethodNewBuildMessageInfo(
    io::Printer* printer) {
  printer->Indent();

  // Collect field info into a sequence of UTF-16 chars. It will be embedded
  // as a Java string in the generated code.
  std::vector<uint16> chars;

  int flags = 0;
  if (SupportFieldPresence(descriptor_->file())) {
    flags |= 0x1;
  }
  if (descriptor_->options().message_set_wire_format()) {
    flags |= 0x2;
  }
  WriteIntToUtf16CharSequence(flags, &chars);
  WriteIntToUtf16CharSequence(descriptor_->field_count(), &chars);

  if (descriptor_->field_count() == 0) {
    printer->Print("java.lang.Object[] objects = null;");
  } else {
    // A single array of all fields (including oneof, oneofCase, hasBits).
    printer->Print("java.lang.Object[] objects = new java.lang.Object[] {\n");
    printer->Indent();

    // Record the number of oneofs.
    WriteIntToUtf16CharSequence(descriptor_->oneof_decl_count(), &chars);
    for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
      const OneofDescriptor* oneof = descriptor_->oneof_decl(i);
      printer->Print(
          "\"$oneof_name$_\",\n"
          "\"$oneof_name$Case_\",\n",
          "oneof_name", context_->GetOneofGeneratorInfo(oneof)->name);
    }

    // Integers for bit fields.
    int total_bits = 0;
    for (int i = 0; i < descriptor_->field_count(); i++) {
      total_bits +=
          field_generators_.get(descriptor_->field(i)).GetNumBitsForMessage();
    }
    int total_ints = (total_bits + 31) / 32;
    for (int i = 0; i < total_ints; i++) {
      printer->Print("\"$bit_field_name$\",\n", "bit_field_name",
                     GetBitFieldName(i));
    }
    WriteIntToUtf16CharSequence(total_ints, &chars);

    int map_count = 0;
    int repeated_count = 0;
    std::unique_ptr<const FieldDescriptor*[]> sorted_fields(
        SortFieldsByNumber(descriptor_));
    for (int i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor* field = sorted_fields[i];
      if (field->is_map()) {
        map_count++;
      } else if (field->is_repeated()) {
        repeated_count++;
      }
    }

    WriteIntToUtf16CharSequence(sorted_fields[0]->number(), &chars);
    WriteIntToUtf16CharSequence(
        sorted_fields[descriptor_->field_count() - 1]->number(), &chars);
    WriteIntToUtf16CharSequence(descriptor_->field_count(), &chars);
    WriteIntToUtf16CharSequence(map_count, &chars);
    WriteIntToUtf16CharSequence(repeated_count, &chars);

    std::vector<const FieldDescriptor*> fields_for_is_initialized_check;
    for (int i = 0; i < descriptor_->field_count(); i++) {
      if (descriptor_->field(i)->is_required() ||
          (GetJavaType(descriptor_->field(i)) == JAVATYPE_MESSAGE &&
           HasRequiredFields(descriptor_->field(i)->message_type()))) {
        fields_for_is_initialized_check.push_back(descriptor_->field(i));
      }
    }
    WriteIntToUtf16CharSequence(fields_for_is_initialized_check.size(), &chars);

    for (int i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor* field = sorted_fields[i];
      field_generators_.get(field).GenerateFieldInfo(printer, &chars);
    }
    printer->Outdent();
    printer->Print("};\n");
  }

  printer->Print("java.lang.String info =\n");
  std::string line;
  for (size_t i = 0; i < chars.size(); i++) {
    uint16 code = chars[i];
    EscapeUtf16ToString(code, &line);
    if (line.size() >= 80) {
      printer->Print("    \"$string$\" +\n", "string", line);
      line.clear();
    }
  }
  printer->Print("    \"$string$\";\n", "string", line);

  printer->Print("return newMessageInfo(DEFAULT_INSTANCE, info, objects);\n");
  printer->Outdent();
}

// ===================================================================

void ImmutableMessageLiteGenerator::GenerateParseFromMethods(
    io::Printer* printer) {
  printer->Print(
      "public static $classname$ parseFrom(\n"
      "    java.nio.ByteBuffer data)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    java.nio.ByteBuffer data,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data, extensionRegistry);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    com.google.protobuf.ByteString data)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    com.google.protobuf.ByteString data,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data, extensionRegistry);\n"
      "}\n"
      "public static $classname$ parseFrom(byte[] data)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    byte[] data,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws com.google.protobuf.InvalidProtocolBufferException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, data, extensionRegistry);\n"
      "}\n"
      "public static $classname$ parseFrom(java.io.InputStream input)\n"
      "    throws java.io.IOException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, input);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    java.io.InputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws java.io.IOException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, input, extensionRegistry);\n"
      "}\n"
      "public static $classname$ parseDelimitedFrom(java.io.InputStream "
      "input)\n"
      "    throws java.io.IOException {\n"
      "  return parseDelimitedFrom(DEFAULT_INSTANCE, input);\n"
      "}\n"
      "public static $classname$ parseDelimitedFrom(\n"
      "    java.io.InputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws java.io.IOException {\n"
      "  return parseDelimitedFrom(DEFAULT_INSTANCE, input, "
      "extensionRegistry);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    com.google.protobuf.CodedInputStream input)\n"
      "    throws java.io.IOException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, input);\n"
      "}\n"
      "public static $classname$ parseFrom(\n"
      "    com.google.protobuf.CodedInputStream input,\n"
      "    com.google.protobuf.ExtensionRegistryLite extensionRegistry)\n"
      "    throws java.io.IOException {\n"
      "  return com.google.protobuf.GeneratedMessageLite.parseFrom(\n"
      "      DEFAULT_INSTANCE, input, extensionRegistry);\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
}

// ===================================================================

void ImmutableMessageLiteGenerator::GenerateBuilder(io::Printer* printer) {
  printer->Print(
      "public static Builder newBuilder() {\n"
      "  return (Builder) DEFAULT_INSTANCE.createBuilder();\n"
      "}\n"
      "public static Builder newBuilder($classname$ prototype) {\n"
      "  return (Builder) DEFAULT_INSTANCE.createBuilder(prototype);\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));

  MessageBuilderLiteGenerator builderGenerator(descriptor_, context_);
  builderGenerator.Generate(printer);
}

// ===================================================================

void ImmutableMessageLiteGenerator::GenerateDynamicMethodNewBuilder(
    io::Printer* printer) {
  printer->Print("return new Builder();\n");
}

// ===================================================================

void ImmutableMessageLiteGenerator::GenerateExtensionRegistrationCode(
    io::Printer* printer) {
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    ImmutableExtensionLiteGenerator(descriptor_->extension(i), context_)
        .GenerateRegistrationCode(printer);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    ImmutableMessageLiteGenerator(descriptor_->nested_type(i), context_)
        .GenerateExtensionRegistrationCode(printer);
  }
}

// ===================================================================
void ImmutableMessageLiteGenerator::GenerateConstructor(io::Printer* printer) {
  printer->Print("private $classname$() {\n", "classname", descriptor_->name());
  printer->Indent();

  // Initialize all fields to default.
  GenerateInitializers(printer);

  printer->Outdent();
  printer->Print("}\n");
}

// ===================================================================
void ImmutableMessageLiteGenerator::GenerateParser(io::Printer* printer) {
  printer->Print(
      "private static volatile com.google.protobuf.Parser<$classname$> "
      "PARSER;\n"
      "\n"
      "public static com.google.protobuf.Parser<$classname$> parser() {\n"
      "  return DEFAULT_INSTANCE.getParserForType();\n"
      "}\n",
      "classname", descriptor_->name());
}

// ===================================================================
void ImmutableMessageLiteGenerator::GenerateInitializers(io::Printer* printer) {
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (!descriptor_->field(i)->containing_oneof()) {
      field_generators_.get(descriptor_->field(i))
          .GenerateInitializationCode(printer);
    }
  }
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
