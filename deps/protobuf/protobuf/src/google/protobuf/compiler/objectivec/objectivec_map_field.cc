// Protocol Buffers - Google's data interchange format
// Copyright 2015 Google Inc.  All rights reserved.
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

#include <map>
#include <string>

#include <google/protobuf/compiler/objectivec/objectivec_map_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

// MapFieldGenerator uses RepeatedFieldGenerator as the parent because it
// provides a bunch of things (no has* methods, comments for contained type,
// etc.).

namespace {

const char* MapEntryTypeName(const FieldDescriptor* descriptor, bool isKey) {
  ObjectiveCType type = GetObjectiveCType(descriptor);
  switch (type) {
    case OBJECTIVECTYPE_INT32:
      return "Int32";
    case OBJECTIVECTYPE_UINT32:
      return "UInt32";
    case OBJECTIVECTYPE_INT64:
      return "Int64";
    case OBJECTIVECTYPE_UINT64:
      return "UInt64";
    case OBJECTIVECTYPE_FLOAT:
      return "Float";
    case OBJECTIVECTYPE_DOUBLE:
      return "Double";
    case OBJECTIVECTYPE_BOOLEAN:
      return "Bool";
    case OBJECTIVECTYPE_STRING:
      return (isKey ? "String" : "Object");
    case OBJECTIVECTYPE_DATA:
      return "Object";
    case OBJECTIVECTYPE_ENUM:
      return "Enum";
    case OBJECTIVECTYPE_MESSAGE:
      return "Object";
  }

  // Some compilers report reaching end of function even though all cases of
  // the enum are handed in the switch.
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

}  // namespace

MapFieldGenerator::MapFieldGenerator(const FieldDescriptor* descriptor,
                                     const Options& options)
    : RepeatedFieldGenerator(descriptor, options) {
  const FieldDescriptor* key_descriptor =
      descriptor->message_type()->FindFieldByName("key");
  const FieldDescriptor* value_descriptor =
      descriptor->message_type()->FindFieldByName("value");
  value_field_generator_.reset(FieldGenerator::Make(value_descriptor, options));

  // Pull over some variables_ from the value.
  variables_["field_type"] = value_field_generator_->variable("field_type");
  variables_["default"] = value_field_generator_->variable("default");
  variables_["default_name"] = value_field_generator_->variable("default_name");

  // Build custom field flags.
  std::vector<string> field_flags;
  field_flags.push_back("GPBFieldMapKey" + GetCapitalizedType(key_descriptor));
  // Pull over the current text format custom name values that was calculated.
  if (variables_["fieldflags"].find("GPBFieldTextFormatNameCustom") !=
      string::npos) {
    field_flags.push_back("GPBFieldTextFormatNameCustom");
  }
  // Pull over some info from the value's flags.
  const string& value_field_flags =
      value_field_generator_->variable("fieldflags");
  if (value_field_flags.find("GPBFieldHasDefaultValue") != string::npos) {
    field_flags.push_back("GPBFieldHasDefaultValue");
  }
  if (value_field_flags.find("GPBFieldHasEnumDescriptor") != string::npos) {
    field_flags.push_back("GPBFieldHasEnumDescriptor");
  }
  variables_["fieldflags"] = BuildFlagsString(FLAGTYPE_FIELD, field_flags);

  ObjectiveCType value_objc_type = GetObjectiveCType(value_descriptor);
  const bool value_is_object_type =
      ((value_objc_type == OBJECTIVECTYPE_STRING) ||
       (value_objc_type == OBJECTIVECTYPE_DATA) ||
       (value_objc_type == OBJECTIVECTYPE_MESSAGE));
  if ((GetObjectiveCType(key_descriptor) == OBJECTIVECTYPE_STRING) &&
      value_is_object_type) {
    variables_["array_storage_type"] = "NSMutableDictionary";
    variables_["array_property_type"] =
        "NSMutableDictionary<NSString*, " +
        value_field_generator_->variable("storage_type") + "*>";
  } else {
    string class_name("GPB");
    class_name += MapEntryTypeName(key_descriptor, true);
    class_name += MapEntryTypeName(value_descriptor, false);
    class_name += "Dictionary";
    variables_["array_storage_type"] = class_name;
    if (value_is_object_type) {
      variables_["array_property_type"] =
          class_name + "<" +
          value_field_generator_->variable("storage_type") + "*>";
    }
  }

  variables_["dataTypeSpecific_name"] =
      value_field_generator_->variable("dataTypeSpecific_name");
  variables_["dataTypeSpecific_value"] =
      value_field_generator_->variable("dataTypeSpecific_value");
}

MapFieldGenerator::~MapFieldGenerator() {}

void MapFieldGenerator::FinishInitialization(void) {
  RepeatedFieldGenerator::FinishInitialization();
  // Use the array_comment support in RepeatedFieldGenerator to output what the
  // values in the map are.
  const FieldDescriptor* value_descriptor =
      descriptor_->message_type()->FindFieldByName("value");
  if (GetObjectiveCType(value_descriptor) == OBJECTIVECTYPE_ENUM) {
    variables_["array_comment"] =
        "// |" + variables_["name"] + "| values are |" + value_field_generator_->variable("storage_type") + "|\n";
  }
}

void MapFieldGenerator::DetermineForwardDeclarations(
    std::set<string>* fwd_decls) const {
  RepeatedFieldGenerator::DetermineForwardDeclarations(fwd_decls);
  const FieldDescriptor* value_descriptor =
      descriptor_->message_type()->FindFieldByName("value");
  if (GetObjectiveCType(value_descriptor) == OBJECTIVECTYPE_MESSAGE) {
    const string& value_storage_type =
        value_field_generator_->variable("storage_type");
    fwd_decls->insert("@class " + value_storage_type);
  }
}


}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
