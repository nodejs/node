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

#include <iostream>

#include <google/protobuf/compiler/objectivec/objectivec_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/compiler/objectivec/objectivec_enum_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_map_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_message_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_primitive_field.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

namespace {

void SetCommonFieldVariables(const FieldDescriptor* descriptor,
                             std::map<string, string>* variables) {
  string camel_case_name = FieldName(descriptor);
  string raw_field_name;
  if (descriptor->type() == FieldDescriptor::TYPE_GROUP) {
    raw_field_name = descriptor->message_type()->name();
  } else {
    raw_field_name = descriptor->name();
  }
  // The logic here has to match -[GGPBFieldDescriptor textFormatName].
  const string un_camel_case_name(
      UnCamelCaseFieldName(camel_case_name, descriptor));
  const bool needs_custom_name = (raw_field_name != un_camel_case_name);

  SourceLocation location;
  if (descriptor->GetSourceLocation(&location)) {
    (*variables)["comments"] = BuildCommentsString(location, true);
  } else {
    (*variables)["comments"] = "\n";
  }
  const string& classname = ClassName(descriptor->containing_type());
  (*variables)["classname"] = classname;
  (*variables)["name"] = camel_case_name;
  const string& capitalized_name = FieldNameCapitalized(descriptor);
  (*variables)["capitalized_name"] = capitalized_name;
  (*variables)["raw_field_name"] = raw_field_name;
  (*variables)["field_number_name"] =
      classname + "_FieldNumber_" + capitalized_name;
  (*variables)["field_number"] = StrCat(descriptor->number());
  (*variables)["field_type"] = GetCapitalizedType(descriptor);
  (*variables)["deprecated_attribute"] = GetOptionalDeprecatedAttribute(descriptor);
  std::vector<string> field_flags;
  if (descriptor->is_repeated()) field_flags.push_back("GPBFieldRepeated");
  if (descriptor->is_required()) field_flags.push_back("GPBFieldRequired");
  if (descriptor->is_optional()) field_flags.push_back("GPBFieldOptional");
  if (descriptor->is_packed()) field_flags.push_back("GPBFieldPacked");

  // ObjC custom flags.
  if (descriptor->has_default_value())
    field_flags.push_back("GPBFieldHasDefaultValue");
  if (needs_custom_name) field_flags.push_back("GPBFieldTextFormatNameCustom");
  if (descriptor->type() == FieldDescriptor::TYPE_ENUM) {
    field_flags.push_back("GPBFieldHasEnumDescriptor");
  }

  (*variables)["fieldflags"] = BuildFlagsString(FLAGTYPE_FIELD, field_flags);

  (*variables)["default"] = DefaultValue(descriptor);
  (*variables)["default_name"] = GPBGenericValueFieldName(descriptor);

  (*variables)["dataTypeSpecific_name"] = "className";
  (*variables)["dataTypeSpecific_value"] = "NULL";

  (*variables)["storage_offset_value"] =
      "(uint32_t)offsetof(" + classname + "__storage_, " + camel_case_name + ")";
  (*variables)["storage_offset_comment"] = "";

  // Clear some common things so they can be set just when needed.
  (*variables)["storage_attribute"] = "";
}

}  // namespace

FieldGenerator* FieldGenerator::Make(const FieldDescriptor* field,
                                     const Options& options) {
  FieldGenerator* result = NULL;
  if (field->is_repeated()) {
    switch (GetObjectiveCType(field)) {
      case OBJECTIVECTYPE_MESSAGE: {
        if (field->is_map()) {
          result = new MapFieldGenerator(field, options);
        } else {
          result = new RepeatedMessageFieldGenerator(field, options);
        }
        break;
      }
      case OBJECTIVECTYPE_ENUM:
        result = new RepeatedEnumFieldGenerator(field, options);
        break;
      default:
        result = new RepeatedPrimitiveFieldGenerator(field, options);
        break;
    }
  } else {
    switch (GetObjectiveCType(field)) {
      case OBJECTIVECTYPE_MESSAGE: {
        result = new MessageFieldGenerator(field, options);
        break;
      }
      case OBJECTIVECTYPE_ENUM:
        result = new EnumFieldGenerator(field, options);
        break;
      default:
        if (IsReferenceType(field)) {
          result = new PrimitiveObjFieldGenerator(field, options);
        } else {
          result = new PrimitiveFieldGenerator(field, options);
        }
        break;
    }
  }
  result->FinishInitialization();
  return result;
}

FieldGenerator::FieldGenerator(const FieldDescriptor* descriptor,
                               const Options& options)
    : descriptor_(descriptor) {
  SetCommonFieldVariables(descriptor, &variables_);
}

FieldGenerator::~FieldGenerator() {}

void FieldGenerator::GenerateFieldNumberConstant(io::Printer* printer) const {
  printer->Print(
      variables_,
      "$field_number_name$ = $field_number$,\n");
}

void FieldGenerator::GenerateCFunctionDeclarations(
    io::Printer* printer) const {
  // Nothing
}

void FieldGenerator::GenerateCFunctionImplementations(
    io::Printer* printer) const {
  // Nothing
}

void FieldGenerator::DetermineForwardDeclarations(
    std::set<string>* fwd_decls) const {
  // Nothing
}

void FieldGenerator::GenerateFieldDescription(
    io::Printer* printer, bool include_default) const {
  // Printed in the same order as the structure decl.
  if (include_default) {
    printer->Print(
        variables_,
        "{\n"
        "  .defaultValue.$default_name$ = $default$,\n"
        "  .core.name = \"$name$\",\n"
        "  .core.dataTypeSpecific.$dataTypeSpecific_name$ = $dataTypeSpecific_value$,\n"
        "  .core.number = $field_number_name$,\n"
        "  .core.hasIndex = $has_index$,\n"
        "  .core.offset = $storage_offset_value$,$storage_offset_comment$\n"
        "  .core.flags = $fieldflags$,\n"
        "  .core.dataType = GPBDataType$field_type$,\n"
        "},\n");
  } else {
    printer->Print(
        variables_,
        "{\n"
        "  .name = \"$name$\",\n"
        "  .dataTypeSpecific.$dataTypeSpecific_name$ = $dataTypeSpecific_value$,\n"
        "  .number = $field_number_name$,\n"
        "  .hasIndex = $has_index$,\n"
        "  .offset = $storage_offset_value$,$storage_offset_comment$\n"
        "  .flags = $fieldflags$,\n"
        "  .dataType = GPBDataType$field_type$,\n"
        "},\n");
  }
}

void FieldGenerator::SetRuntimeHasBit(int has_index) {
  variables_["has_index"] = StrCat(has_index);
}

void FieldGenerator::SetNoHasBit(void) {
  variables_["has_index"] = "GPBNoHasBit";
}

int FieldGenerator::ExtraRuntimeHasBitsNeeded(void) const {
  return 0;
}

void FieldGenerator::SetExtraRuntimeHasBitsBase(int index_base) {
  // NOTE: src/google/protobuf/compiler/plugin.cc makes use of cerr for some
  // error cases, so it seems to be ok to use as a back door for errors.
  std::cerr << "Error: should have overridden SetExtraRuntimeHasBitsBase()." << std::endl;
  std::cerr.flush();
  abort();
}

void FieldGenerator::SetOneofIndexBase(int index_base) {
  if (descriptor_->containing_oneof() != NULL) {
    int index = descriptor_->containing_oneof()->index() + index_base;
    // Flip the sign to mark it as a oneof.
    variables_["has_index"] = StrCat(-index);
  }
}

void FieldGenerator::FinishInitialization(void) {
  // If "property_type" wasn't set, make it "storage_type".
  if ((variables_.find("property_type") == variables_.end()) &&
      (variables_.find("storage_type") != variables_.end())) {
    variables_["property_type"] = variable("storage_type");
  }
}

SingleFieldGenerator::SingleFieldGenerator(const FieldDescriptor* descriptor,
                                           const Options& options)
    : FieldGenerator(descriptor, options) {
  // Nothing
}

SingleFieldGenerator::~SingleFieldGenerator() {}

void SingleFieldGenerator::GenerateFieldStorageDeclaration(
    io::Printer* printer) const {
  printer->Print(variables_, "$storage_type$ $name$;\n");
}

void SingleFieldGenerator::GeneratePropertyDeclaration(
    io::Printer* printer) const {
  printer->Print(variables_, "$comments$");
  printer->Print(
      variables_,
      "@property(nonatomic, readwrite) $property_type$ $name$$deprecated_attribute$;\n"
      "\n");
  if (WantsHasProperty()) {
    printer->Print(
        variables_,
        "@property(nonatomic, readwrite) BOOL has$capitalized_name$$deprecated_attribute$;\n");
  }
}

void SingleFieldGenerator::GeneratePropertyImplementation(
    io::Printer* printer) const {
  if (WantsHasProperty()) {
    printer->Print(variables_, "@dynamic has$capitalized_name$, $name$;\n");
  } else {
    printer->Print(variables_, "@dynamic $name$;\n");
  }
}

bool SingleFieldGenerator::WantsHasProperty(void) const {
  if (descriptor_->containing_oneof() != NULL) {
    // If in a oneof, it uses the oneofcase instead of a has bit.
    return false;
  }
  if (HasFieldPresence(descriptor_->file())) {
    // In proto1/proto2, every field has a has_$name$() method.
    return true;
  }
  return false;
}

bool SingleFieldGenerator::RuntimeUsesHasBit(void) const {
  if (descriptor_->containing_oneof() != NULL) {
    // The oneof tracks what is set instead.
    return false;
  }
  return true;
}

ObjCObjFieldGenerator::ObjCObjFieldGenerator(const FieldDescriptor* descriptor,
                                             const Options& options)
    : SingleFieldGenerator(descriptor, options) {
  variables_["property_storage_attribute"] = "strong";
  if (IsRetainedName(variables_["name"])) {
    variables_["storage_attribute"] = " NS_RETURNS_NOT_RETAINED";
  }
}

ObjCObjFieldGenerator::~ObjCObjFieldGenerator() {}

void ObjCObjFieldGenerator::GenerateFieldStorageDeclaration(
    io::Printer* printer) const {
  printer->Print(variables_, "$storage_type$ *$name$;\n");
}

void ObjCObjFieldGenerator::GeneratePropertyDeclaration(
    io::Printer* printer) const {

  // Differs from SingleFieldGenerator::GeneratePropertyDeclaration() in that
  // it uses pointers and deals with Objective C's rules around storage name
  // conventions (init*, new*, etc.)

  printer->Print(variables_, "$comments$");
  printer->Print(
      variables_,
      "@property(nonatomic, readwrite, $property_storage_attribute$, null_resettable) $property_type$ *$name$$storage_attribute$$deprecated_attribute$;\n");
  if (WantsHasProperty()) {
    printer->Print(
        variables_,
        "/** Test to see if @c $name$ has been set. */\n"
        "@property(nonatomic, readwrite) BOOL has$capitalized_name$$deprecated_attribute$;\n");
  }
  if (IsInitName(variables_.find("name")->second)) {
    // If property name starts with init we need to annotate it to get past ARC.
    // http://stackoverflow.com/questions/18723226/how-do-i-annotate-an-objective-c-property-with-an-objc-method-family/18723227#18723227
    printer->Print(variables_,
                   "- ($property_type$ *)$name$ GPB_METHOD_FAMILY_NONE$deprecated_attribute$;\n");
  }
  printer->Print("\n");
}

RepeatedFieldGenerator::RepeatedFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : ObjCObjFieldGenerator(descriptor, options) {
  // Default to no comment and let the cases needing it fill it in.
  variables_["array_comment"] = "";
}

RepeatedFieldGenerator::~RepeatedFieldGenerator() {}

void RepeatedFieldGenerator::FinishInitialization(void) {
  FieldGenerator::FinishInitialization();
  if (variables_.find("array_property_type") == variables_.end()) {
    variables_["array_property_type"] = variable("array_storage_type");
  }
}

void RepeatedFieldGenerator::GenerateFieldStorageDeclaration(
    io::Printer* printer) const {
  printer->Print(variables_, "$array_storage_type$ *$name$;\n");
}

void RepeatedFieldGenerator::GeneratePropertyImplementation(
    io::Printer* printer) const {
  printer->Print(variables_, "@dynamic $name$, $name$_Count;\n");
}

void RepeatedFieldGenerator::GeneratePropertyDeclaration(
    io::Printer* printer) const {

  // Repeated fields don't need the has* properties, but they do expose a
  // *Count (to check without autocreation).  So for the field property we need
  // the same logic as ObjCObjFieldGenerator::GeneratePropertyDeclaration() for
  // dealing with needing Objective C's rules around storage name conventions
  // (init*, new*, etc.)

  printer->Print(
      variables_,
      "$comments$"
      "$array_comment$"
      "@property(nonatomic, readwrite, strong, null_resettable) $array_property_type$ *$name$$storage_attribute$$deprecated_attribute$;\n"
      "/** The number of items in @c $name$ without causing the array to be created. */\n"
      "@property(nonatomic, readonly) NSUInteger $name$_Count$deprecated_attribute$;\n");
  if (IsInitName(variables_.find("name")->second)) {
    // If property name starts with init we need to annotate it to get past ARC.
    // http://stackoverflow.com/questions/18723226/how-do-i-annotate-an-objective-c-property-with-an-objc-method-family/18723227#18723227
    printer->Print(variables_,
                   "- ($array_property_type$ *)$name$ GPB_METHOD_FAMILY_NONE$deprecated_attribute$;\n");
  }
  printer->Print("\n");
}

bool RepeatedFieldGenerator::WantsHasProperty(void) const {
  // Consumer check the array size/existance rather than a has bit.
  return false;
}

bool RepeatedFieldGenerator::RuntimeUsesHasBit(void) const {
  return false;  // The array having anything is what is used.
}

FieldGeneratorMap::FieldGeneratorMap(const Descriptor* descriptor,
                                     const Options& options)
    : descriptor_(descriptor),
      field_generators_(descriptor->field_count()),
      extension_generators_(descriptor->extension_count()) {
  // Construct all the FieldGenerators.
  for (int i = 0; i < descriptor->field_count(); i++) {
    field_generators_[i].reset(
        FieldGenerator::Make(descriptor->field(i), options));
  }
  for (int i = 0; i < descriptor->extension_count(); i++) {
    extension_generators_[i].reset(
        FieldGenerator::Make(descriptor->extension(i), options));
  }
}

FieldGeneratorMap::~FieldGeneratorMap() {}

const FieldGenerator& FieldGeneratorMap::get(
    const FieldDescriptor* field) const {
  GOOGLE_CHECK_EQ(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}

const FieldGenerator& FieldGeneratorMap::get_extension(int index) const {
  return *extension_generators_[index];
}

int FieldGeneratorMap::CalculateHasBits(void) {
  int total_bits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (field_generators_[i]->RuntimeUsesHasBit()) {
      field_generators_[i]->SetRuntimeHasBit(total_bits);
      ++total_bits;
    } else {
      field_generators_[i]->SetNoHasBit();
    }
    int extra_bits = field_generators_[i]->ExtraRuntimeHasBitsNeeded();
    if (extra_bits) {
      field_generators_[i]->SetExtraRuntimeHasBitsBase(total_bits);
      total_bits += extra_bits;
    }
  }
  return total_bits;
}

void FieldGeneratorMap::SetOneofIndexBase(int index_base) {
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_[i]->SetOneofIndexBase(index_base);
  }
}

bool FieldGeneratorMap::DoesAnyFieldHaveNonZeroDefault(void) const {
  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (HasNonZeroDefaultValue(descriptor_->field(i))) {
      return true;
    }
  }

  return false;
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
