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

#include <google/protobuf/compiler/cpp/cpp_enum_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void SetEnumVariables(const FieldDescriptor* descriptor,
                      std::map<std::string, std::string>* variables,
                      const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  const EnumValueDescriptor* default_value = descriptor->default_value_enum();
  (*variables)["type"] = QualifiedClassName(descriptor->enum_type(), options);
  (*variables)["default"] = Int32ToString(default_value->number());
  (*variables)["full_name"] = descriptor->full_name();
}

}  // namespace

// ===================================================================

EnumFieldGenerator::EnumFieldGenerator(const FieldDescriptor* descriptor,
                                       const Options& options)
    : FieldGenerator(descriptor, options) {
  SetEnumVariables(descriptor, &variables_, options);
}

EnumFieldGenerator::~EnumFieldGenerator() {}

void EnumFieldGenerator::GeneratePrivateMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("int $name$_;\n");
}

void EnumFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$$type$ ${1$$name$$}$() const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$($type$ value);\n",
      descriptor_);
}

void EnumFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return static_cast< $type$ >($name$_);\n"
      "}\n"
      "inline void $classname$::set_$name$($type$ value) {\n");
  if (!HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("  assert($type$_IsValid(value));\n");
  }
  format(
      "  $set_hasbit$\n"
      "  $name$_ = value;\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n");
}

void EnumFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = $default$;\n");
}

void EnumFieldGenerator::GenerateMergingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("set_$name$(from.$name$());\n");
}

void EnumFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("swap($name$_, other->$name$_);\n");
}

void EnumFieldGenerator::GenerateConstructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = $default$;\n");
}

void EnumFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = from.$name$_;\n");
}

void EnumFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "int value = 0;\n"
      "DO_((::$proto_ns$::internal::WireFormatLite::ReadPrimitive<\n"
      "         int, ::$proto_ns$::internal::WireFormatLite::TYPE_ENUM>(\n"
      "       input, &value)));\n");
  if (HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("set_$name$(static_cast< $type$ >(value));\n");
  } else {
    format(
        "if ($type$_IsValid(value)) {\n"
        "  set_$name$(static_cast< $type$ >(value));\n");
    if (UseUnknownFieldSet(descriptor_->file(), options_)) {
      format(
          "} else {\n"
          "  mutable_unknown_fields()->AddVarint(\n"
          "      $number$, static_cast<$uint64$>(value));\n");
    } else {
      format(
          "} else {\n"
          "  unknown_fields_stream.WriteVarint32($1$u);\n"
          "  unknown_fields_stream.WriteVarint32(\n"
          "      static_cast<$uint32$>(value));\n",
          internal::WireFormat::MakeTag(descriptor_));
    }
    format("}\n");
  }
}

void EnumFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "::$proto_ns$::internal::WireFormatLite::WriteEnum(\n"
      "  $number$, this->$name$(), output);\n");
}

void EnumFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "target = ::$proto_ns$::internal::WireFormatLite::WriteEnumToArray(\n"
      "  $number$, this->$name$(), target);\n");
}

void EnumFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ +\n"
      "  ::$proto_ns$::internal::WireFormatLite::EnumSize(this->$name$());\n");
}

// ===================================================================

EnumOneofFieldGenerator::EnumOneofFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : EnumFieldGenerator(descriptor, options) {
  SetCommonOneofFieldVariables(descriptor, &variables_);
}

EnumOneofFieldGenerator::~EnumOneofFieldGenerator() {}

void EnumOneofFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  if (has_$name$()) {\n"
      "    return static_cast< $type$ >($field_member$);\n"
      "  }\n"
      "  return static_cast< $type$ >($default$);\n"
      "}\n"
      "inline void $classname$::set_$name$($type$ value) {\n");
  if (!HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("  assert($type$_IsValid(value));\n");
  }
  format(
      "  if (!has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "  }\n"
      "  $field_member$ = value;\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n");
}

void EnumOneofFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$field_member$ = $default$;\n");
}

void EnumOneofFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void EnumOneofFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$ns$::_$classname$_default_instance_.$name$_ = $default$;\n");
}

// ===================================================================

RepeatedEnumFieldGenerator::RepeatedEnumFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : FieldGenerator(descriptor, options) {
  SetEnumVariables(descriptor, &variables_, options);
}

RepeatedEnumFieldGenerator::~RepeatedEnumFieldGenerator() {}

void RepeatedEnumFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("::$proto_ns$::RepeatedField<int> $name$_;\n");
  if (descriptor_->is_packed() &&
      HasGeneratedMethods(descriptor_->file(), options_)) {
    format("mutable std::atomic<int> _$name$_cached_byte_size_;\n");
  }
}

void RepeatedEnumFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$$type$ ${1$$name$$}$(int index) const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, $type$ value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$($type$ value);\n"
      "$deprecated_attr$const ::$proto_ns$::RepeatedField<int>& "
      "${1$$name$$}$() const;\n"
      "$deprecated_attr$::$proto_ns$::RepeatedField<int>* "
      "${1$mutable_$name$$}$();\n",
      descriptor_);
}

void RepeatedEnumFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$(int index) const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return static_cast< $type$ >($name$_.Get(index));\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, $type$ value) {\n");
  if (!HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("  assert($type$_IsValid(value));\n");
  }
  format(
      "  $name$_.Set(index, value);\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$($type$ value) {\n");
  if (!HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("  assert($type$_IsValid(value));\n");
  }
  format(
      "  $name$_.Add(value);\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline const ::$proto_ns$::RepeatedField<int>&\n"
      "$classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_list:$full_name$)\n"
      "  return $name$_;\n"
      "}\n"
      "inline ::$proto_ns$::RepeatedField<int>*\n"
      "$classname$::mutable_$name$() {\n"
      "  // @@protoc_insertion_point(field_mutable_list:$full_name$)\n"
      "  return &$name$_;\n"
      "}\n");
}

void RepeatedEnumFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.Clear();\n");
}

void RepeatedEnumFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.MergeFrom(from.$name$_);\n");
}

void RepeatedEnumFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.InternalSwap(&other->$name$_);\n");
}

void RepeatedEnumFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedEnumFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // Don't use ReadRepeatedPrimitive here so that the enum can be validated.
  format(
      "int value = 0;\n"
      "DO_((::$proto_ns$::internal::WireFormatLite::ReadPrimitive<\n"
      "         int, ::$proto_ns$::internal::WireFormatLite::TYPE_ENUM>(\n"
      "       input, &value)));\n");
  if (HasPreservingUnknownEnumSemantics(descriptor_)) {
    format("add_$name$(static_cast< $type$ >(value));\n");
  } else {
    format(
        "if ($type$_IsValid(value)) {\n"
        "  add_$name$(static_cast< $type$ >(value));\n");
    if (UseUnknownFieldSet(descriptor_->file(), options_)) {
      format(
          "} else {\n"
          "  mutable_unknown_fields()->AddVarint(\n"
          "      $number$, static_cast<$uint64$>(value));\n");
    } else {
      format(
          "} else {\n"
          "  unknown_fields_stream.WriteVarint32(tag);\n"
          "  unknown_fields_stream.WriteVarint32(\n"
          "      static_cast<$uint32$>(value));\n");
    }
    format("}\n");
  }
}

void RepeatedEnumFieldGenerator::GenerateMergeFromCodedStreamWithPacking(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->is_packed()) {
    // This path is rarely executed, so we use a non-inlined implementation.
    if (HasPreservingUnknownEnumSemantics(descriptor_)) {
      format(
          "DO_((::$proto_ns$::internal::"
          "WireFormatLite::ReadPackedEnumPreserveUnknowns(\n"
          "       input,\n"
          "       $number$,\n"
          "       nullptr,\n"
          "       nullptr,\n"
          "       this->mutable_$name$())));\n");
    } else if (UseUnknownFieldSet(descriptor_->file(), options_)) {
      format(
          "DO_((::$proto_ns$::internal::WireFormat::"
          "ReadPackedEnumPreserveUnknowns(\n"
          "       input,\n"
          "       $number$,\n"
          "       $type$_IsValid,\n"
          "       mutable_unknown_fields(),\n"
          "       this->mutable_$name$())));\n");
    } else {
      format(
          "DO_((::$proto_ns$::internal::"
          "WireFormatLite::ReadPackedEnumPreserveUnknowns(\n"
          "       input,\n"
          "       $number$,\n"
          "       $type$_IsValid,\n"
          "       &unknown_fields_stream,\n"
          "       this->mutable_$name$())));\n");
    }
  } else {
    format(
        "$uint32$ length;\n"
        "DO_(input->ReadVarint32(&length));\n"
        "::$proto_ns$::io::CodedInputStream::Limit limit = "
        "input->PushLimit(static_cast<int>(length));\n"
        "while (input->BytesUntilLimit() > 0) {\n"
        "  int value = 0;\n"
        "  DO_((::$proto_ns$::internal::WireFormatLite::ReadPrimitive<\n"
        "         int, ::$proto_ns$::internal::WireFormatLite::TYPE_ENUM>(\n"
        "       input, &value)));\n");
    if (HasPreservingUnknownEnumSemantics(descriptor_)) {
      format("  add_$name$(static_cast< $type$ >(value));\n");
    } else {
      format(
          "  if ($type$_IsValid(value)) {\n"
          "    add_$name$(static_cast< $type$ >(value));\n"
          "  } else {\n");
      if (UseUnknownFieldSet(descriptor_->file(), options_)) {
        format(
            "  mutable_unknown_fields()->AddVarint(\n"
            "      $number$, static_cast<$uint64$>(value));\n");
      } else {
        format(
            "    unknown_fields_stream.WriteVarint32(tag);\n"
            "    unknown_fields_stream.WriteVarint32(\n"
            "        static_cast<$uint32$>(value));\n");
      }
      format("  }\n");
    }
    format(
        "}\n"
        "input->PopLimit(limit);\n");
  }
}

void RepeatedEnumFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->is_packed()) {
    // Write the tag and the size.
    format(
        "if (this->$name$_size() > 0) {\n"
        "  ::$proto_ns$::internal::WireFormatLite::WriteTag(\n"
        "    $number$,\n"
        "    "
        "::$proto_ns$::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
        "    output);\n"
        "  output->WriteVarint32(_$name$_cached_byte_size_.load(\n"
        "      std::memory_order_relaxed));\n"
        "}\n");
  }
  format("for (int i = 0, n = this->$name$_size(); i < n; i++) {\n");
  if (descriptor_->is_packed()) {
    format(
        "  ::$proto_ns$::internal::WireFormatLite::WriteEnumNoTag(\n"
        "    this->$name$(i), output);\n");
  } else {
    format(
        "  ::$proto_ns$::internal::WireFormatLite::WriteEnum(\n"
        "    $number$, this->$name$(i), output);\n");
  }
  format("}\n");
}

void RepeatedEnumFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->is_packed()) {
    // Write the tag and the size.
    format(
        "if (this->$name$_size() > 0) {\n"
        "  target = ::$proto_ns$::internal::WireFormatLite::WriteTagToArray(\n"
        "    $number$,\n"
        "    "
        "::$proto_ns$::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,\n"
        "    target);\n"
        "  target = ::$proto_ns$::io::CodedOutputStream::WriteVarint32ToArray("
        "      _$name$_cached_byte_size_.load(std::memory_order_relaxed),\n"
        "      target);\n"
        "  target = "
        "::$proto_ns$::internal::WireFormatLite::WriteEnumNoTagToArray(\n"
        "    this->$name$_, target);\n"
        "}\n");
  } else {
    format(
        "target = ::$proto_ns$::internal::WireFormatLite::WriteEnumToArray(\n"
        "  $number$, this->$name$_, target);\n");
  }
}

void RepeatedEnumFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "{\n"
      "  size_t data_size = 0;\n"
      "  unsigned int count = static_cast<unsigned int>(this->$name$_size());");
  format.Indent();
  format(
      "for (unsigned int i = 0; i < count; i++) {\n"
      "  data_size += ::$proto_ns$::internal::WireFormatLite::EnumSize(\n"
      "    this->$name$(static_cast<int>(i)));\n"
      "}\n");

  if (descriptor_->is_packed()) {
    format(
        "if (data_size > 0) {\n"
        "  total_size += $tag_size$ +\n"
        "    ::$proto_ns$::internal::WireFormatLite::Int32Size(\n"
        "        static_cast<$int32$>(data_size));\n"
        "}\n"
        "int cached_size = ::$proto_ns$::internal::ToCachedSize(data_size);\n"
        "_$name$_cached_byte_size_.store(cached_size,\n"
        "                                std::memory_order_relaxed);\n"
        "total_size += data_size;\n");
  } else {
    format("total_size += ($tag_size$UL * count) + data_size;\n");
  }
  format.Outdent();
  format("}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
