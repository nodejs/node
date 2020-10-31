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

#include <map>
#include <string>

#include <google/protobuf/compiler/objectivec/objectivec_enum.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>
#include <algorithm> // std::find()

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

EnumGenerator::EnumGenerator(const EnumDescriptor* descriptor)
    : descriptor_(descriptor),
      name_(EnumName(descriptor_)) {
  // Track the names for the enum values, and if an alias overlaps a base
  // value, skip making a name for it. Likewise if two alias overlap, the
  // first one wins.
  // The one gap in this logic is if two base values overlap, but for that
  // to happen you have to have "Foo" and "FOO" or "FOO_BAR" and "FooBar",
  // and if an enum has that, it is already going to be confusing and a
  // compile error is just fine.
  // The values are still tracked to support the reflection apis and
  // TextFormat handing since they are different there.
  std::set<std::string> value_names;

  for (int i = 0; i < descriptor_->value_count(); i++) {
    const EnumValueDescriptor* value = descriptor_->value(i);
    const EnumValueDescriptor* canonical_value =
        descriptor_->FindValueByNumber(value->number());

    if (value == canonical_value) {
      base_values_.push_back(value);
      value_names.insert(EnumValueName(value));
    } else {
      string value_name(EnumValueName(value));
      if (value_names.find(value_name) != value_names.end()) {
        alias_values_to_skip_.insert(value);
      } else {
        value_names.insert(value_name);
      }
    }
    all_values_.push_back(value);
  }
}

EnumGenerator::~EnumGenerator() {}

void EnumGenerator::GenerateHeader(io::Printer* printer) {
  string enum_comments;
  SourceLocation location;
  if (descriptor_->GetSourceLocation(&location)) {
    enum_comments = BuildCommentsString(location, true);
  } else {
    enum_comments = "";
  }

  printer->Print(
      "#pragma mark - Enum $name$\n"
      "\n",
      "name", name_);

  // Swift 5 included SE0192 "Handling Future Enum Cases"
  //   https://github.com/apple/swift-evolution/blob/master/proposals/0192-non-exhaustive-enums.md
  // Since a .proto file can get new values added to an enum at any time, they
  // are effectively "non-frozen". Even in a proto3 syntax file where there is
  // support for the unknown value, an edit to the file can always add a new
  // value moving something from unknown to known. Since Swift is now ABI
  // stable, it also means a binary could contain Swift compiled against one
  // version of the .pbobjc.h file, but finally linked against an enum with
  // more cases. So the Swift code will always have to treat ObjC Proto Enums
  // as "non-frozen". The default behavior in SE0192 is for all objc enums to
  // be "non-frozen" unless marked as otherwise, so this means this generation
  // doesn't have to bother with the `enum_extensibility` attribute, as the
  // default will be what is needed.

  printer->Print("$comments$typedef$deprecated_attribute$ GPB_ENUM($name$) {\n",
                 "comments", enum_comments,
                 "deprecated_attribute", GetOptionalDeprecatedAttribute(descriptor_, descriptor_->file()),
                 "name", name_);
  printer->Indent();

  if (HasPreservingUnknownEnumSemantics(descriptor_->file())) {
    // Include the unknown value.
    printer->Print(
      "/**\n"
      " * Value used if any message's field encounters a value that is not defined\n"
      " * by this enum. The message will also have C functions to get/set the rawValue\n"
      " * of the field.\n"
      " **/\n"
      "$name$_GPBUnrecognizedEnumeratorValue = kGPBUnrecognizedEnumeratorValue,\n",
      "name", name_);
  }
  for (int i = 0; i < all_values_.size(); i++) {
    if (alias_values_to_skip_.find(all_values_[i]) != alias_values_to_skip_.end()) {
      continue;
    }
    SourceLocation location;
    if (all_values_[i]->GetSourceLocation(&location)) {
      string comments = BuildCommentsString(location, true).c_str();
      if (comments.length() > 0) {
        if (i > 0) {
          printer->Print("\n");
        }
        printer->Print(comments.c_str());
      }
    }

    printer->Print(
        "$name$$deprecated_attribute$ = $value$,\n",
        "name", EnumValueName(all_values_[i]),
        "deprecated_attribute", GetOptionalDeprecatedAttribute(all_values_[i]),
        "value", StrCat(all_values_[i]->number()));
  }
  printer->Outdent();
  printer->Print(
      "};\n"
      "\n"
      "GPBEnumDescriptor *$name$_EnumDescriptor(void);\n"
      "\n"
      "/**\n"
      " * Checks to see if the given value is defined by the enum or was not known at\n"
      " * the time this source was generated.\n"
      " **/\n"
      "BOOL $name$_IsValidValue(int32_t value);\n"
      "\n",
      "name", name_);
}

void EnumGenerator::GenerateSource(io::Printer* printer) {
  printer->Print(
      "#pragma mark - Enum $name$\n"
      "\n",
      "name", name_);

  // Note: For the TextFormat decode info, we can't use the enum value as
  // the key because protocol buffer enums have 'allow_alias', which lets
  // a value be used more than once. Instead, the index into the list of
  // enum value descriptions is used. Note: start with -1 so the first one
  // will be zero.
  TextFormatDecodeData text_format_decode_data;
  int enum_value_description_key = -1;
  string text_blob;

  for (int i = 0; i < all_values_.size(); i++) {
    ++enum_value_description_key;
    string short_name(EnumValueShortName(all_values_[i]));
    text_blob += short_name + '\0';
    if (UnCamelCaseEnumShortName(short_name) != all_values_[i]->name()) {
      text_format_decode_data.AddString(enum_value_description_key, short_name,
                                        all_values_[i]->name());
    }
  }

  printer->Print(
      "GPBEnumDescriptor *$name$_EnumDescriptor(void) {\n"
      "  static _Atomic(GPBEnumDescriptor*) descriptor = nil;\n"
      "  if (!descriptor) {\n",
      "name", name_);

  static const int kBytesPerLine = 40;  // allow for escaping
  printer->Print(
      "    static const char *valueNames =");
  for (int i = 0; i < text_blob.size(); i += kBytesPerLine) {
    printer->Print(
        "\n        \"$data$\"",
        "data", EscapeTrigraphs(CEscape(text_blob.substr(i, kBytesPerLine))));
  }
  printer->Print(
      ";\n"
      "    static const int32_t values[] = {\n");
  for (int i = 0; i < all_values_.size(); i++) {
    printer->Print("        $name$,\n",  "name", EnumValueName(all_values_[i]));
  }
  printer->Print("    };\n");

  if (text_format_decode_data.num_entries() == 0) {
    printer->Print(
        "    GPBEnumDescriptor *worker =\n"
        "        [GPBEnumDescriptor allocDescriptorForName:GPBNSStringifySymbol($name$)\n"
        "                                       valueNames:valueNames\n"
        "                                           values:values\n"
        "                                            count:(uint32_t)(sizeof(values) / sizeof(int32_t))\n"
        "                                     enumVerifier:$name$_IsValidValue];\n",
        "name", name_);
    } else {
      printer->Print(
        "    static const char *extraTextFormatInfo = \"$extraTextFormatInfo$\";\n"
        "    GPBEnumDescriptor *worker =\n"
        "        [GPBEnumDescriptor allocDescriptorForName:GPBNSStringifySymbol($name$)\n"
        "                                       valueNames:valueNames\n"
        "                                           values:values\n"
        "                                            count:(uint32_t)(sizeof(values) / sizeof(int32_t))\n"
        "                                     enumVerifier:$name$_IsValidValue\n"
        "                              extraTextFormatInfo:extraTextFormatInfo];\n",
        "name", name_,
        "extraTextFormatInfo", CEscape(text_format_decode_data.Data()));
    }
    printer->Print(
      "    GPBEnumDescriptor *expected = nil;\n"
      "    if (!atomic_compare_exchange_strong(&descriptor, &expected, worker)) {\n"
      "      [worker release];\n"
      "    }\n"
      "  }\n"
      "  return descriptor;\n"
      "}\n\n");

  printer->Print(
      "BOOL $name$_IsValidValue(int32_t value__) {\n"
      "  switch (value__) {\n",
      "name", name_);

  for (int i = 0; i < base_values_.size(); i++) {
    printer->Print(
        "    case $name$:\n",
        "name", EnumValueName(base_values_[i]));
  }

  printer->Print(
      "      return YES;\n"
      "    default:\n"
      "      return NO;\n"
      "  }\n"
      "}\n\n");
}
}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
