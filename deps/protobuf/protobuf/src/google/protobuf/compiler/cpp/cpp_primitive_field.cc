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

#include <google/protobuf/compiler/cpp/cpp_primitive_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

using internal::WireFormatLite;

namespace {

// For encodings with fixed sizes, returns that size in bytes.  Otherwise
// returns -1.
int FixedSize(FieldDescriptor::Type type) {
  switch (type) {
    case FieldDescriptor::TYPE_INT32:
      return -1;
    case FieldDescriptor::TYPE_INT64:
      return -1;
    case FieldDescriptor::TYPE_UINT32:
      return -1;
    case FieldDescriptor::TYPE_UINT64:
      return -1;
    case FieldDescriptor::TYPE_SINT32:
      return -1;
    case FieldDescriptor::TYPE_SINT64:
      return -1;
    case FieldDescriptor::TYPE_FIXED32:
      return WireFormatLite::kFixed32Size;
    case FieldDescriptor::TYPE_FIXED64:
      return WireFormatLite::kFixed64Size;
    case FieldDescriptor::TYPE_SFIXED32:
      return WireFormatLite::kSFixed32Size;
    case FieldDescriptor::TYPE_SFIXED64:
      return WireFormatLite::kSFixed64Size;
    case FieldDescriptor::TYPE_FLOAT:
      return WireFormatLite::kFloatSize;
    case FieldDescriptor::TYPE_DOUBLE:
      return WireFormatLite::kDoubleSize;

    case FieldDescriptor::TYPE_BOOL:
      return WireFormatLite::kBoolSize;
    case FieldDescriptor::TYPE_ENUM:
      return -1;

    case FieldDescriptor::TYPE_STRING:
      return -1;
    case FieldDescriptor::TYPE_BYTES:
      return -1;
    case FieldDescriptor::TYPE_GROUP:
      return -1;
    case FieldDescriptor::TYPE_MESSAGE:
      return -1;

      // No default because we want the compiler to complain if any new
      // types are added.
  }
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return -1;
}

void SetPrimitiveVariables(const FieldDescriptor* descriptor,
                           std::map<std::string, std::string>* variables,
                           const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["type"] = PrimitiveTypeName(options, descriptor->cpp_type());
  (*variables)["default"] = DefaultValue(options, descriptor);
  (*variables)["tag"] = StrCat(internal::WireFormat::MakeTag(descriptor));
  int fixed_size = FixedSize(descriptor->type());
  if (fixed_size != -1) {
    (*variables)["fixed_size"] = StrCat(fixed_size);
  }
  (*variables)["wire_format_field_type"] = FieldDescriptorProto_Type_Name(
      static_cast<FieldDescriptorProto_Type>(descriptor->type()));
  (*variables)["full_name"] = descriptor->full_name();
}

}  // namespace

// ===================================================================

PrimitiveFieldGenerator::PrimitiveFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : FieldGenerator(descriptor, options) {
  SetPrimitiveVariables(descriptor, &variables_, options);
}

PrimitiveFieldGenerator::~PrimitiveFieldGenerator() {}

void PrimitiveFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$type$ $name$_;\n");
}

void PrimitiveFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$$type$ ${1$$name$$}$() const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$($type$ value);\n",
      descriptor_);
}

void PrimitiveFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return $name$_;\n"
      "}\n"
      "inline void $classname$::set_$name$($type$ value) {\n"
      "  $set_hasbit$\n"
      "  $name$_ = value;\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n");
}

void PrimitiveFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = $default$;\n");
}

void PrimitiveFieldGenerator::GenerateMergingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("set_$name$(from.$name$());\n");
}

void PrimitiveFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("swap($name$_, other->$name$_);\n");
}

void PrimitiveFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = $default$;\n");
}

void PrimitiveFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = from.$name$_;\n");
}

void PrimitiveFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$set_hasbit_io$\n"
      "DO_((::$proto_ns$::internal::WireFormatLite::ReadPrimitive<\n"
      "         $type$, "
      "::$proto_ns$::internal::WireFormatLite::$wire_format_field_type$>(\n"
      "       input, &$name$_)));\n");
}

void PrimitiveFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "::$proto_ns$::internal::WireFormatLite::Write$declared_type$("
      "$number$, this->$name$(), output);\n");
}

void PrimitiveFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "target = "
      "::$proto_ns$::internal::WireFormatLite::Write$declared_type$ToArray("
      "$number$, this->$name$(), target);\n");
}

void PrimitiveFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  int fixed_size = FixedSize(descriptor_->type());
  if (fixed_size == -1) {
    format(
        "total_size += $tag_size$ +\n"
        "  ::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
        "    this->$name$());\n");
  } else {
    format("total_size += $tag_size$ + $fixed_size$;\n");
  }
}

// ===================================================================

PrimitiveOneofFieldGenerator::PrimitiveOneofFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : PrimitiveFieldGenerator(descriptor, options) {
  SetCommonOneofFieldVariables(descriptor, &variables_);
}

PrimitiveOneofFieldGenerator::~PrimitiveOneofFieldGenerator() {}

void PrimitiveOneofFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  if (has_$name$()) {\n"
      "    return $field_member$;\n"
      "  }\n"
      "  return $default$;\n"
      "}\n"
      "inline void $classname$::set_$name$($type$ value) {\n"
      "  if (!has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "  }\n"
      "  $field_member$ = value;\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n");
}

void PrimitiveOneofFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$field_member$ = $default$;\n");
}

void PrimitiveOneofFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void PrimitiveOneofFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$ns$::_$classname$_default_instance_.$name$_ = $default$;\n");
}

void PrimitiveOneofFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "clear_$oneof_name$();\n"
      "DO_((::$proto_ns$::internal::WireFormatLite::ReadPrimitive<\n"
      "         $type$, "
      "::$proto_ns$::internal::WireFormatLite::$wire_format_field_type$>(\n"
      "       input, &$field_member$)));\n"
      "set_has_$name$();\n");
}

// ===================================================================

RepeatedPrimitiveFieldGenerator::RepeatedPrimitiveFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : FieldGenerator(descriptor, options) {
  SetPrimitiveVariables(descriptor, &variables_, options);

  if (descriptor->is_packed()) {
    variables_["packed_reader"] = "ReadPackedPrimitive";
    variables_["repeated_reader"] = "ReadRepeatedPrimitiveNoInline";
  } else {
    variables_["packed_reader"] = "ReadPackedPrimitiveNoInline";
    variables_["repeated_reader"] = "ReadRepeatedPrimitive";
  }
}

RepeatedPrimitiveFieldGenerator::~RepeatedPrimitiveFieldGenerator() {}

void RepeatedPrimitiveFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("::$proto_ns$::RepeatedField< $type$ > $name$_;\n");
  if (descriptor_->is_packed() &&
      HasGeneratedMethods(descriptor_->file(), options_)) {
    format("mutable std::atomic<int> _$name$_cached_byte_size_;\n");
  }
}

void RepeatedPrimitiveFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$$type$ ${1$$name$$}$(int index) const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, $type$ value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$($type$ value);\n"
      "$deprecated_attr$const ::$proto_ns$::RepeatedField< $type$ >&\n"
      "    ${1$$name$$}$() const;\n"
      "$deprecated_attr$::$proto_ns$::RepeatedField< $type$ >*\n"
      "    ${1$mutable_$name$$}$();\n",
      descriptor_);
}

void RepeatedPrimitiveFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$ $classname$::$name$(int index) const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return $name$_.Get(index);\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, $type$ value) {\n"
      "  $name$_.Set(index, value);\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$($type$ value) {\n"
      "  $name$_.Add(value);\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline const ::$proto_ns$::RepeatedField< $type$ >&\n"
      "$classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_list:$full_name$)\n"
      "  return $name$_;\n"
      "}\n"
      "inline ::$proto_ns$::RepeatedField< $type$ >*\n"
      "$classname$::mutable_$name$() {\n"
      "  // @@protoc_insertion_point(field_mutable_list:$full_name$)\n"
      "  return &$name$_;\n"
      "}\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.Clear();\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.MergeFrom(from.$name$_);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.InternalSwap(&other->$name$_);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedPrimitiveFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.CopyFrom(from.$name$_);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "DO_((::$proto_ns$::internal::WireFormatLite::$repeated_reader$<\n"
      "         $type$, "
      "::$proto_ns$::internal::WireFormatLite::$wire_format_field_type$>(\n"
      "       $tag_size$, $tag$u, input, this->mutable_$name$())));\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateMergeFromCodedStreamWithPacking(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "DO_((::$proto_ns$::internal::WireFormatLite::$packed_reader$<\n"
      "         $type$, "
      "::$proto_ns$::internal::WireFormatLite::$wire_format_field_type$>(\n"
      "       input, this->mutable_$name$())));\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  bool array_written = false;
  if (descriptor_->is_packed()) {
    // Write the tag and the size.
    format(
        "if (this->$name$_size() > 0) {\n"
        "  ::$proto_ns$::internal::WireFormatLite::WriteTag("
        "$number$, "
        "::$proto_ns$::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED, "
        "output);\n"
        "  output->WriteVarint32(_$name$_cached_byte_size_.load(\n"
        "      std::memory_order_relaxed));\n");

    if (FixedSize(descriptor_->type()) > 0) {
      // TODO(ckennelly): Use RepeatedField<T>::unsafe_data() via
      // WireFormatLite to access the contents of this->$name$_ to save a branch
      // here.
      format(
          "  "
          "::$proto_ns$::internal::WireFormatLite::Write$declared_type$Array(\n"
          "    this->$name$().data(), this->$name$_size(), output);\n");
      array_written = true;  // Wrote array all at once
    }
    format("}\n");
  }
  if (!array_written) {
    format("for (int i = 0, n = this->$name$_size(); i < n; i++) {\n");
    if (descriptor_->is_packed()) {
      format(
          "  "
          "::$proto_ns$::internal::WireFormatLite::Write$declared_type$NoTag(\n"
          "    this->$name$(i), output);\n");
    } else {
      format(
          "  ::$proto_ns$::internal::WireFormatLite::Write$declared_type$(\n"
          "    $number$, this->$name$(i), output);\n");
    }
    format("}\n");
  }
}

void RepeatedPrimitiveFieldGenerator::GenerateSerializeWithCachedSizesToArray(
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
        "  target = "
        "::$proto_ns$::io::CodedOutputStream::WriteVarint32ToArray(\n"
        "      _$name$_cached_byte_size_.load(std::memory_order_relaxed),\n"
        "       target);\n"
        "  target = ::$proto_ns$::internal::WireFormatLite::\n"
        "    Write$declared_type$NoTagToArray(this->$name$_, target);\n"
        "}\n");
  } else {
    format(
        "target = ::$proto_ns$::internal::WireFormatLite::\n"
        "  Write$declared_type$ToArray($number$, this->$name$_, target);\n");
  }
}

void RepeatedPrimitiveFieldGenerator::GenerateByteSize(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("{\n");
  format.Indent();
  int fixed_size = FixedSize(descriptor_->type());
  if (fixed_size == -1) {
    format(
        "size_t data_size = ::$proto_ns$::internal::WireFormatLite::\n"
        "  $declared_type$Size(this->$name$_);\n");
  } else {
    format(
        "unsigned int count = static_cast<unsigned int>(this->$name$_size());\n"
        "size_t data_size = $fixed_size$UL * count;\n");
  }

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
    format(
        "total_size += $tag_size$ *\n"
        "              "
        "::$proto_ns$::internal::FromIntSize(this->$name$_size());\n"
        "total_size += data_size;\n");
  }
  format.Outdent();
  format("}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
