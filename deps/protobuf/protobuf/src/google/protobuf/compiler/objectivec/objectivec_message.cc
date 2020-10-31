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

#include <algorithm>
#include <iostream>
#include <sstream>

#include <google/protobuf/compiler/objectivec/objectivec_message.h>
#include <google/protobuf/compiler/objectivec/objectivec_enum.h>
#include <google/protobuf/compiler/objectivec/objectivec_extension.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

using internal::WireFormat;
using internal::WireFormatLite;

namespace {
struct FieldOrderingByNumber {
  inline bool operator()(const FieldDescriptor* a,
                         const FieldDescriptor* b) const {
    return a->number() < b->number();
  }
};

int OrderGroupForFieldDescriptor(const FieldDescriptor* descriptor) {
  // The first item in the object structure is our uint32[] for has bits.
  // We then want to order things to make the instances as small as
  // possible. So we follow the has bits with:
  //   1. Anything always 4 bytes - float, *32, enums
  //   2. Anything that is always a pointer (they will be 8 bytes on 64 bit
  //      builds and 4 bytes on 32bit builds.
  //   3. Anything always 8 bytes - double, *64
  //
  // NOTE: Bools aren't listed, they were stored in the has bits.
  //
  // Why? Using 64bit builds as an example, this means worse case, we have
  // enough bools that we overflow 1 byte from 4 byte alignment, so 3 bytes
  // are wasted before the 4 byte values. Then if we have an odd number of
  // those 4 byte values, the 8 byte values will be pushed down by 32bits to
  // keep them aligned. But the structure will end 8 byte aligned, so no
  // waste on the end. If you did the reverse order, you could waste 4 bytes
  // before the first 8 byte value (after the has array), then a single
  // bool on the end would need 7 bytes of padding to make the overall
  // structure 8 byte aligned; so 11 bytes, wasted total.

  // Anything repeated is a GPB*Array/NSArray, so pointer.
  if (descriptor->is_repeated()) {
    return 3;
  }

  switch (descriptor->type()) {
    // All always 8 bytes.
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_FIXED64:
      return 4;

    // Pointers (string and bytes are NSString and NSData); 8 or 4 bytes
    // depending on the build architecture.
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_BYTES:
      return 3;

    // All always 4 bytes (enums are int32s).
    case FieldDescriptor::TYPE_FLOAT:
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_ENUM:
      return 2;

    // 0 bytes. Stored in the has bits.
    case FieldDescriptor::TYPE_BOOL:
      return 99;  // End of the list (doesn't really matter).
  }

  // Some compilers report reaching end of function even though all cases of
  // the enum are handed in the switch.
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return 0;
}

struct FieldOrderingByStorageSize {
  inline bool operator()(const FieldDescriptor* a,
                         const FieldDescriptor* b) const {
    // Order by grouping.
    const int order_group_a = OrderGroupForFieldDescriptor(a);
    const int order_group_b = OrderGroupForFieldDescriptor(b);
    if (order_group_a != order_group_b) {
      return order_group_a < order_group_b;
    }
    // Within the group, order by field number (provides stable ordering).
    return a->number() < b->number();
  }
};

struct ExtensionRangeOrdering {
  bool operator()(const Descriptor::ExtensionRange* a,
                  const Descriptor::ExtensionRange* b) const {
    return a->start < b->start;
  }
};

// Sort the fields of the given Descriptor by number into a new[]'d array
// and return it.
const FieldDescriptor** SortFieldsByNumber(const Descriptor* descriptor) {
  const FieldDescriptor** fields =
      new const FieldDescriptor* [descriptor->field_count()];
  for (int i = 0; i < descriptor->field_count(); i++) {
    fields[i] = descriptor->field(i);
  }
  std::sort(fields, fields + descriptor->field_count(), FieldOrderingByNumber());
  return fields;
}

// Sort the fields of the given Descriptor by storage size into a new[]'d
// array and return it.
const FieldDescriptor** SortFieldsByStorageSize(const Descriptor* descriptor) {
  const FieldDescriptor** fields =
      new const FieldDescriptor* [descriptor->field_count()];
  for (int i = 0; i < descriptor->field_count(); i++) {
    fields[i] = descriptor->field(i);
  }
  std::sort(fields, fields + descriptor->field_count(),
       FieldOrderingByStorageSize());
  return fields;
}
}  // namespace

MessageGenerator::MessageGenerator(const string& root_classname,
                                   const Descriptor* descriptor,
                                   const Options& options)
    : root_classname_(root_classname),
      descriptor_(descriptor),
      field_generators_(descriptor, options),
      class_name_(ClassName(descriptor_)),
      deprecated_attribute_(
          GetOptionalDeprecatedAttribute(descriptor, descriptor->file(), false, true)) {

  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extension_generators_.emplace_back(
        new ExtensionGenerator(class_name_, descriptor_->extension(i)));
  }

  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    OneofGenerator* generator = new OneofGenerator(descriptor_->oneof_decl(i));
    oneof_generators_.emplace_back(generator);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    EnumGenerator* generator = new EnumGenerator(descriptor_->enum_type(i));
    enum_generators_.emplace_back(generator);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    MessageGenerator* generator =
        new MessageGenerator(root_classname_,
                             descriptor_->nested_type(i),
                             options);
    nested_message_generators_.emplace_back(generator);
  }
}

MessageGenerator::~MessageGenerator() {}

void MessageGenerator::GenerateStaticVariablesInitialization(
    io::Printer* printer) {
  for (const auto& generator : extension_generators_) {
    generator->GenerateStaticVariablesInitialization(printer);
  }

  for (const auto& generator : nested_message_generators_) {
    generator->GenerateStaticVariablesInitialization(printer);
  }
}

void MessageGenerator::DetermineForwardDeclarations(std::set<string>* fwd_decls) {
  if (!IsMapEntryMessage(descriptor_)) {
    for (int i = 0; i < descriptor_->field_count(); i++) {
      const FieldDescriptor* fieldDescriptor = descriptor_->field(i);
      field_generators_.get(fieldDescriptor)
          .DetermineForwardDeclarations(fwd_decls);
    }
  }

  for (const auto& generator : nested_message_generators_) {
    generator->DetermineForwardDeclarations(fwd_decls);
  }
}

bool MessageGenerator::IncludesOneOfDefinition() const {
  if (!oneof_generators_.empty()) {
    return true;
  }

  for (const auto& generator : nested_message_generators_) {
    if (generator->IncludesOneOfDefinition()) {
      return true;
    }
  }

  return false;
}

void MessageGenerator::GenerateEnumHeader(io::Printer* printer) {
  for (const auto& generator : enum_generators_) {
    generator->GenerateHeader(printer);
  }

  for (const auto& generator : nested_message_generators_) {
    generator->GenerateEnumHeader(printer);
  }
}

void MessageGenerator::GenerateExtensionRegistrationSource(
    io::Printer* printer) {
  for (const auto& generator : extension_generators_) {
    generator->GenerateRegistrationSource(printer);
  }

  for (const auto& generator : nested_message_generators_) {
    generator->GenerateExtensionRegistrationSource(printer);
  }
}

void MessageGenerator::GenerateMessageHeader(io::Printer* printer) {
  // This a a map entry message, just recurse and do nothing directly.
  if (IsMapEntryMessage(descriptor_)) {
    for (const auto& generator : nested_message_generators_) {
      generator->GenerateMessageHeader(printer);
    }
    return;
  }

  printer->Print(
      "#pragma mark - $classname$\n"
      "\n",
      "classname", class_name_);

  if (descriptor_->field_count()) {
    std::unique_ptr<const FieldDescriptor*[]> sorted_fields(
        SortFieldsByNumber(descriptor_));

    printer->Print("typedef GPB_ENUM($classname$_FieldNumber) {\n",
                   "classname", class_name_);
    printer->Indent();

    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(sorted_fields[i])
          .GenerateFieldNumberConstant(printer);
    }

    printer->Outdent();
    printer->Print("};\n\n");
  }

  for (const auto& generator : oneof_generators_) {
    generator->GenerateCaseEnum(printer);
  }

  string message_comments;
  SourceLocation location;
  if (descriptor_->GetSourceLocation(&location)) {
    message_comments = BuildCommentsString(location, false);
  } else {
    message_comments = "";
  }

  printer->Print(
      "$comments$$deprecated_attribute$@interface $classname$ : GPBMessage\n\n",
      "classname", class_name_,
      "deprecated_attribute", deprecated_attribute_,
      "comments", message_comments);

  std::vector<char> seen_oneofs(descriptor_->oneof_decl_count(), 0);
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const FieldDescriptor* field = descriptor_->field(i);
    if (field->containing_oneof() != NULL) {
      const int oneof_index = field->containing_oneof()->index();
      if (!seen_oneofs[oneof_index]) {
        seen_oneofs[oneof_index] = 1;
        oneof_generators_[oneof_index]->GeneratePublicCasePropertyDeclaration(
            printer);
      }
    }
    field_generators_.get(field).GeneratePropertyDeclaration(printer);
  }

  printer->Print("@end\n\n");

  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
        .GenerateCFunctionDeclarations(printer);
  }

  if (!oneof_generators_.empty()) {
    for (const auto& generator : oneof_generators_) {
      generator->GenerateClearFunctionDeclaration(printer);
    }
    printer->Print("\n");
  }

  if (descriptor_->extension_count() > 0) {
    printer->Print("@interface $classname$ (DynamicMethods)\n\n",
                   "classname", class_name_);
    for (const auto& generator : extension_generators_) {
      generator->GenerateMembersHeader(printer);
    }
    printer->Print("@end\n\n");
  }

  for (const auto& generator : nested_message_generators_) {
    generator->GenerateMessageHeader(printer);
  }
}

void MessageGenerator::GenerateSource(io::Printer* printer) {
  if (!IsMapEntryMessage(descriptor_)) {
    printer->Print(
        "#pragma mark - $classname$\n"
        "\n",
        "classname", class_name_);

    if (!deprecated_attribute_.empty()) {
      // No warnings when compiling the impl of this deprecated class.
      printer->Print(
          "#pragma clang diagnostic push\n"
          "#pragma clang diagnostic ignored \"-Wdeprecated-implementations\"\n"
          "\n");
    }

    printer->Print("@implementation $classname$\n\n",
                   "classname", class_name_);

    for (const auto& generator : oneof_generators_) {
      generator->GeneratePropertyImplementation(printer);
    }

    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(descriptor_->field(i))
          .GeneratePropertyImplementation(printer);
    }

    std::unique_ptr<const FieldDescriptor*[]> sorted_fields(
        SortFieldsByNumber(descriptor_));
    std::unique_ptr<const FieldDescriptor*[]> size_order_fields(
        SortFieldsByStorageSize(descriptor_));

    std::vector<const Descriptor::ExtensionRange*> sorted_extensions;
    for (int i = 0; i < descriptor_->extension_range_count(); ++i) {
      sorted_extensions.push_back(descriptor_->extension_range(i));
    }

    std::sort(sorted_extensions.begin(), sorted_extensions.end(),
         ExtensionRangeOrdering());

    // Assign has bits:
    // 1. FieldGeneratorMap::CalculateHasBits() loops through the fields seeing
    //    who needs has bits and assigning them.
    // 2. FieldGenerator::SetOneofIndexBase() overrides has_bit with a negative
    //    index that groups all the elements in the oneof.
    size_t num_has_bits = field_generators_.CalculateHasBits();
    size_t sizeof_has_storage = (num_has_bits + 31) / 32;
    if (sizeof_has_storage == 0) {
      // In the case where no field needs has bits, don't let the _has_storage_
      // end up as zero length (zero length arrays are sort of a grey area
      // since it has to be at the start of the struct). This also ensures a
      // field with only oneofs keeps the required negative indices they need.
      sizeof_has_storage = 1;
    }
    // Tell all the fields the oneof base.
    for (const auto& generator : oneof_generators_) {
      generator->SetOneofIndexBase(sizeof_has_storage);
    }
    field_generators_.SetOneofIndexBase(sizeof_has_storage);
    // sizeof_has_storage needs enough bits for the single fields that aren't in
    // any oneof, and then one int32 for each oneof (to store the field number).
    sizeof_has_storage += descriptor_->oneof_decl_count();

    printer->Print(
        "\n"
        "typedef struct $classname$__storage_ {\n"
        "  uint32_t _has_storage_[$sizeof_has_storage$];\n",
        "classname", class_name_,
        "sizeof_has_storage", StrCat(sizeof_has_storage));
    printer->Indent();

    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(size_order_fields[i])
          .GenerateFieldStorageDeclaration(printer);
    }
    printer->Outdent();

    printer->Print("} $classname$__storage_;\n\n", "classname", class_name_);


    printer->Print(
        "// This method is threadsafe because it is initially called\n"
        "// in +initialize for each subclass.\n"
        "+ (GPBDescriptor *)descriptor {\n"
        "  static GPBDescriptor *descriptor = nil;\n"
        "  if (!descriptor) {\n");

    TextFormatDecodeData text_format_decode_data;
    bool has_fields = descriptor_->field_count() > 0;
    bool need_defaults = field_generators_.DoesAnyFieldHaveNonZeroDefault();
    string field_description_type;
    if (need_defaults) {
      field_description_type = "GPBMessageFieldDescriptionWithDefault";
    } else {
      field_description_type = "GPBMessageFieldDescription";
    }
    if (has_fields) {
      printer->Print(
          "    static $field_description_type$ fields[] = {\n",
          "field_description_type", field_description_type);
      printer->Indent();
      printer->Indent();
      printer->Indent();
      for (int i = 0; i < descriptor_->field_count(); ++i) {
        const FieldGenerator& field_generator =
            field_generators_.get(sorted_fields[i]);
        field_generator.GenerateFieldDescription(printer, need_defaults);
        if (field_generator.needs_textformat_name_support()) {
          text_format_decode_data.AddString(sorted_fields[i]->number(),
                                            field_generator.generated_objc_name(),
                                            field_generator.raw_field_name());
        }
      }
      printer->Outdent();
      printer->Outdent();
      printer->Outdent();
      printer->Print(
          "    };\n");
    }

    std::map<string, string> vars;
    vars["classname"] = class_name_;
    vars["rootclassname"] = root_classname_;
    vars["fields"] = has_fields ? "fields" : "NULL";
    if (has_fields) {
      vars["fields_count"] =
          "(uint32_t)(sizeof(fields) / sizeof(" + field_description_type + "))";
    } else {
      vars["fields_count"] = "0";
    }

    std::vector<string> init_flags;
    if (need_defaults) {
      init_flags.push_back("GPBDescriptorInitializationFlag_FieldsWithDefault");
    }
    if (descriptor_->options().message_set_wire_format()) {
      init_flags.push_back("GPBDescriptorInitializationFlag_WireFormat");
    }
    vars["init_flags"] = BuildFlagsString(FLAGTYPE_DESCRIPTOR_INITIALIZATION,
                                          init_flags);

    printer->Print(
        vars,
        "    GPBDescriptor *localDescriptor =\n"
        "        [GPBDescriptor allocDescriptorForClass:[$classname$ class]\n"
        "                                     rootClass:[$rootclassname$ class]\n"
        "                                          file:$rootclassname$_FileDescriptor()\n"
        "                                        fields:$fields$\n"
        "                                    fieldCount:$fields_count$\n"
        "                                   storageSize:sizeof($classname$__storage_)\n"
        "                                         flags:$init_flags$];\n");
    if (oneof_generators_.size() != 0) {
      printer->Print(
          "    static const char *oneofs[] = {\n");
      for (const auto& generator : oneof_generators_) {
        printer->Print("      \"$name$\",\n", "name",
                       generator->DescriptorName());
      }
      printer->Print(
          "    };\n"
          "    [localDescriptor setupOneofs:oneofs\n"
          "                           count:(uint32_t)(sizeof(oneofs) / sizeof(char*))\n"
          "                   firstHasIndex:$first_has_index$];\n",
          "first_has_index", oneof_generators_[0]->HasIndexAsString());
    }
    if (text_format_decode_data.num_entries() != 0) {
      const string text_format_data_str(text_format_decode_data.Data());
      printer->Print(
          "#if !GPBOBJC_SKIP_MESSAGE_TEXTFORMAT_EXTRAS\n"
          "    static const char *extraTextFormatInfo =");
      static const int kBytesPerLine = 40;  // allow for escaping
      for (int i = 0; i < text_format_data_str.size(); i += kBytesPerLine) {
        printer->Print(
            "\n        \"$data$\"",
            "data", EscapeTrigraphs(
                CEscape(text_format_data_str.substr(i, kBytesPerLine))));
      }
      printer->Print(
          ";\n"
          "    [localDescriptor setupExtraTextInfo:extraTextFormatInfo];\n"
          "#endif  // !GPBOBJC_SKIP_MESSAGE_TEXTFORMAT_EXTRAS\n");
    }
    if (sorted_extensions.size() != 0) {
      printer->Print(
          "    static const GPBExtensionRange ranges[] = {\n");
      for (int i = 0; i < sorted_extensions.size(); i++) {
        printer->Print("      { .start = $start$, .end = $end$ },\n",
                       "start", StrCat(sorted_extensions[i]->start),
                       "end", StrCat(sorted_extensions[i]->end));
      }
      printer->Print(
          "    };\n"
          "    [localDescriptor setupExtensionRanges:ranges\n"
          "                                    count:(uint32_t)(sizeof(ranges) / sizeof(GPBExtensionRange))];\n");
    }
    if (descriptor_->containing_type() != NULL) {
      string parent_class_name = ClassName(descriptor_->containing_type());
      printer->Print(
          "    [localDescriptor setupContainingMessageClassName:GPBStringifySymbol($parent_name$)];\n",
          "parent_name", parent_class_name);
    }
    string suffix_added;
    ClassName(descriptor_, &suffix_added);
    if (suffix_added.size() > 0) {
      printer->Print(
          "    [localDescriptor setupMessageClassNameSuffix:@\"$suffix$\"];\n",
          "suffix", suffix_added);
    }
    printer->Print(
        "    #if defined(DEBUG) && DEBUG\n"
        "      NSAssert(descriptor == nil, @\"Startup recursed!\");\n"
        "    #endif  // DEBUG\n"
        "    descriptor = localDescriptor;\n"
        "  }\n"
        "  return descriptor;\n"
        "}\n\n"
        "@end\n\n");

    if (!deprecated_attribute_.empty()) {
      printer->Print(
          "#pragma clang diagnostic pop\n"
          "\n");
    }

    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(descriptor_->field(i))
          .GenerateCFunctionImplementations(printer);
    }

    for (const auto& generator : oneof_generators_) {
      generator->GenerateClearFunctionImplementation(printer);
    }
  }

  for (const auto& generator : enum_generators_) {
    generator->GenerateSource(printer);
  }

  for (const auto& generator : nested_message_generators_) {
    generator->GenerateSource(printer);
  }
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
