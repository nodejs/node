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

#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <memory>

#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_primitive_field.h>
#include <google/protobuf/compiler/cpp/cpp_string_field.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/cpp/cpp_enum_field.h>
#include <google/protobuf/compiler/cpp/cpp_map_field.h>
#include <google/protobuf/compiler/cpp/cpp_message_field.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

using internal::WireFormat;

void SetCommonFieldVariables(const FieldDescriptor* descriptor,
                             std::map<std::string, std::string>* variables,
                             const Options& options) {
  SetCommonVars(options, variables);
  (*variables)["ns"] = Namespace(descriptor, options);
  (*variables)["name"] = FieldName(descriptor);
  (*variables)["index"] = StrCat(descriptor->index());
  (*variables)["number"] = StrCat(descriptor->number());
  (*variables)["classname"] = ClassName(FieldScope(descriptor), false);
  (*variables)["declared_type"] = DeclaredTypeMethodName(descriptor->type());
  (*variables)["field_member"] = FieldName(descriptor) + "_";

  (*variables)["tag_size"] = StrCat(
      WireFormat::TagSize(descriptor->number(), descriptor->type()));
  (*variables)["deprecated_attr"] =
      DeprecatedAttribute(options, descriptor->options().deprecated());

  (*variables)["set_hasbit"] = "";
  (*variables)["clear_hasbit"] = "";
  if (HasFieldPresence(descriptor->file())) {
    (*variables)["set_hasbit_io"] =
        "_Internal::set_has_" + FieldName(descriptor) + "(&_has_bits_);";
  } else {
    (*variables)["set_hasbit_io"] = "";
  }

  // These variables are placeholders to pick out the beginning and ends of
  // identifiers for annotations (when doing so with existing variables would
  // be ambiguous or impossible). They should never be set to anything but the
  // empty string.
  (*variables)["{"] = "";
  (*variables)["}"] = "";
}

void FieldGenerator::SetHasBitIndex(int32 has_bit_index) {
  if (!HasFieldPresence(descriptor_->file()) || has_bit_index == -1) {
    return;
  }
  variables_["set_hasbit"] = StrCat(
      "_has_bits_[", has_bit_index / 32, "] |= 0x",
      strings::Hex(1u << (has_bit_index % 32), strings::ZERO_PAD_8), "u;");
  variables_["clear_hasbit"] = StrCat(
      "_has_bits_[", has_bit_index / 32, "] &= ~0x",
      strings::Hex(1u << (has_bit_index % 32), strings::ZERO_PAD_8), "u;");
}

void SetCommonOneofFieldVariables(
    const FieldDescriptor* descriptor,
    std::map<std::string, std::string>* variables) {
  const std::string prefix = descriptor->containing_oneof()->name() + "_.";
  (*variables)["oneof_name"] = descriptor->containing_oneof()->name();
  (*variables)["field_member"] =
      StrCat(prefix, (*variables)["name"], "_");
}

FieldGenerator::~FieldGenerator() {}

void FieldGenerator::GenerateMergeFromCodedStreamWithPacking(
    io::Printer* printer) const {
  // Reaching here indicates a bug. Cases are:
  //   - This FieldGenerator should support packing, but this method should be
  //     overridden.
  //   - This FieldGenerator doesn't support packing, and this method should
  //     never have been called.
  GOOGLE_LOG(FATAL) << "GenerateMergeFromCodedStreamWithPacking() "
             << "called on field generator that does not support packing.";
}

FieldGeneratorMap::FieldGeneratorMap(const Descriptor* descriptor,
                                     const Options& options,
                                     MessageSCCAnalyzer* scc_analyzer)
    : descriptor_(descriptor), field_generators_(descriptor->field_count()) {
  // Construct all the FieldGenerators.
  for (int i = 0; i < descriptor->field_count(); i++) {
    field_generators_[i].reset(
        MakeGenerator(descriptor->field(i), options, scc_analyzer));
  }
}

FieldGenerator* FieldGeneratorMap::MakeGoogleInternalGenerator(
    const FieldDescriptor* field, const Options& options,
    MessageSCCAnalyzer* scc_analyzer) {

  return nullptr;
}

FieldGenerator* FieldGeneratorMap::MakeGenerator(
    const FieldDescriptor* field, const Options& options,
    MessageSCCAnalyzer* scc_analyzer) {
  FieldGenerator* generator =
      MakeGoogleInternalGenerator(field, options, scc_analyzer);
  if (generator) {
    return generator;
  }

  if (field->is_repeated()) {
    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_MESSAGE:
        if (field->is_map()) {
          return new MapFieldGenerator(field, options);
        } else {
          return new RepeatedMessageFieldGenerator(field, options,
                                                   scc_analyzer);
        }
      case FieldDescriptor::CPPTYPE_STRING:
        return new RepeatedStringFieldGenerator(field, options);
      case FieldDescriptor::CPPTYPE_ENUM:
        return new RepeatedEnumFieldGenerator(field, options);
      default:
        return new RepeatedPrimitiveFieldGenerator(field, options);
    }
  } else if (field->containing_oneof()) {
    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_MESSAGE:
        return new MessageOneofFieldGenerator(field, options, scc_analyzer);
      case FieldDescriptor::CPPTYPE_STRING:
        return new StringOneofFieldGenerator(field, options);
      case FieldDescriptor::CPPTYPE_ENUM:
        return new EnumOneofFieldGenerator(field, options);
      default:
        return new PrimitiveOneofFieldGenerator(field, options);
    }
  } else {
    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_MESSAGE:
        return new MessageFieldGenerator(field, options, scc_analyzer);
      case FieldDescriptor::CPPTYPE_STRING:
        return new StringFieldGenerator(field, options);
      case FieldDescriptor::CPPTYPE_ENUM:
        return new EnumFieldGenerator(field, options);
      default:
        return new PrimitiveFieldGenerator(field, options);
    }
  }
}

FieldGeneratorMap::~FieldGeneratorMap() {}

const FieldGenerator& FieldGeneratorMap::get(
    const FieldDescriptor* field) const {
  GOOGLE_CHECK_EQ(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
