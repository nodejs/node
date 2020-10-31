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

// Author: jschorr@google.com (Joseph Schorr)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/text_format.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <climits>
#include <limits>
#include <vector>

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/any.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/strtod.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/map_field.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/strutil.h>



#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/stl_util.h>

#include <google/protobuf/port_def.inc>


namespace google {
namespace protobuf {

namespace {

inline bool IsHexNumber(const std::string& str) {
  return (str.length() >= 2 && str[0] == '0' &&
          (str[1] == 'x' || str[1] == 'X'));
}

inline bool IsOctNumber(const std::string& str) {
  return (str.length() >= 2 && str[0] == '0' &&
          (str[1] >= '0' && str[1] < '8'));
}

}  // namespace

std::string Message::DebugString() const {
  std::string debug_string;

  TextFormat::Printer printer;
  printer.SetExpandAny(true);

  printer.PrintToString(*this, &debug_string);

  return debug_string;
}

std::string Message::ShortDebugString() const {
  std::string debug_string;

  TextFormat::Printer printer;
  printer.SetSingleLineMode(true);
  printer.SetExpandAny(true);

  printer.PrintToString(*this, &debug_string);
  // Single line mode currently might have an extra space at the end.
  if (debug_string.size() > 0 && debug_string[debug_string.size() - 1] == ' ') {
    debug_string.resize(debug_string.size() - 1);
  }

  return debug_string;
}

std::string Message::Utf8DebugString() const {
  std::string debug_string;

  TextFormat::Printer printer;
  printer.SetUseUtf8StringEscaping(true);
  printer.SetExpandAny(true);

  printer.PrintToString(*this, &debug_string);

  return debug_string;
}

void Message::PrintDebugString() const { printf("%s", DebugString().c_str()); }


// ===========================================================================
// Implementation of the parse information tree class.
TextFormat::ParseInfoTree::ParseInfoTree() {}

TextFormat::ParseInfoTree::~ParseInfoTree() {
  // Remove any nested information trees, as they are owned by this tree.
  for (NestedMap::iterator it = nested_.begin(); it != nested_.end(); ++it) {
    STLDeleteElements(&(it->second));
  }
}

void TextFormat::ParseInfoTree::RecordLocation(
    const FieldDescriptor* field, TextFormat::ParseLocation location) {
  locations_[field].push_back(location);
}

TextFormat::ParseInfoTree* TextFormat::ParseInfoTree::CreateNested(
    const FieldDescriptor* field) {
  // Owned by us in the map.
  TextFormat::ParseInfoTree* instance = new TextFormat::ParseInfoTree();
  std::vector<TextFormat::ParseInfoTree*>* trees = &nested_[field];
  GOOGLE_CHECK(trees);
  trees->push_back(instance);
  return instance;
}

void CheckFieldIndex(const FieldDescriptor* field, int index) {
  if (field == nullptr) {
    return;
  }

  if (field->is_repeated() && index == -1) {
    GOOGLE_LOG(DFATAL) << "Index must be in range of repeated field values. "
                << "Field: " << field->name();
  } else if (!field->is_repeated() && index != -1) {
    GOOGLE_LOG(DFATAL) << "Index must be -1 for singular fields."
                << "Field: " << field->name();
  }
}

TextFormat::ParseLocation TextFormat::ParseInfoTree::GetLocation(
    const FieldDescriptor* field, int index) const {
  CheckFieldIndex(field, index);
  if (index == -1) {
    index = 0;
  }

  const std::vector<TextFormat::ParseLocation>* locations =
      FindOrNull(locations_, field);
  if (locations == nullptr || index >= locations->size()) {
    return TextFormat::ParseLocation();
  }

  return (*locations)[index];
}

TextFormat::ParseInfoTree* TextFormat::ParseInfoTree::GetTreeForNested(
    const FieldDescriptor* field, int index) const {
  CheckFieldIndex(field, index);
  if (index == -1) {
    index = 0;
  }

  const std::vector<TextFormat::ParseInfoTree*>* trees =
      FindOrNull(nested_, field);
  if (trees == nullptr || index >= trees->size()) {
    return nullptr;
  }

  return (*trees)[index];
}

namespace {
// These functions implement the behavior of the "default" TextFormat::Finder,
// they are defined as standalone to be called when finder_ is nullptr.
const FieldDescriptor* DefaultFinderFindExtension(Message* message,
                                                  const std::string& name) {
  const Descriptor* descriptor = message->GetDescriptor();
  return descriptor->file()->pool()->FindExtensionByPrintableName(descriptor,
                                                                  name);
}

const FieldDescriptor* DefaultFinderFindExtensionByNumber(
    const Descriptor* descriptor, int number) {
  return descriptor->file()->pool()->FindExtensionByNumber(descriptor, number);
}

const Descriptor* DefaultFinderFindAnyType(const Message& message,
                                           const std::string& prefix,
                                           const std::string& name) {
  if (prefix != internal::kTypeGoogleApisComPrefix &&
      prefix != internal::kTypeGoogleProdComPrefix) {
    return nullptr;
  }
  return message.GetDescriptor()->file()->pool()->FindMessageTypeByName(name);
}
}  // namespace

// ===========================================================================
// Internal class for parsing an ASCII representation of a Protocol Message.
// This class makes use of the Protocol Message compiler's tokenizer found
// in //net/proto2/io/public/tokenizer.h. Note that class's Parse
// method is *not* thread-safe and should only be used in a single thread at
// a time.

// Makes code slightly more readable.  The meaning of "DO(foo)" is
// "Execute foo and fail if it fails.", where failure is indicated by
// returning false. Borrowed from parser.cc (Thanks Kenton!).
#define DO(STATEMENT) \
  if (STATEMENT) {    \
  } else {            \
    return false;     \
  }

class TextFormat::Parser::ParserImpl {
 public:
  // Determines if repeated values for non-repeated fields and
  // oneofs are permitted, e.g., the string "foo: 1 foo: 2" for a
  // required/optional field named "foo", or "baz: 1 qux: 2"
  // where "baz" and "qux" are members of the same oneof.
  enum SingularOverwritePolicy {
    ALLOW_SINGULAR_OVERWRITES = 0,   // the last value is retained
    FORBID_SINGULAR_OVERWRITES = 1,  // an error is issued
  };

  ParserImpl(const Descriptor* root_message_type,
             io::ZeroCopyInputStream* input_stream,
             io::ErrorCollector* error_collector,
             const TextFormat::Finder* finder, ParseInfoTree* parse_info_tree,
             SingularOverwritePolicy singular_overwrite_policy,
             bool allow_case_insensitive_field, bool allow_unknown_field,
             bool allow_unknown_extension, bool allow_unknown_enum,
             bool allow_field_number, bool allow_relaxed_whitespace,
             bool allow_partial, int recursion_limit)
      : error_collector_(error_collector),
        finder_(finder),
        parse_info_tree_(parse_info_tree),
        tokenizer_error_collector_(this),
        tokenizer_(input_stream, &tokenizer_error_collector_),
        root_message_type_(root_message_type),
        singular_overwrite_policy_(singular_overwrite_policy),
        allow_case_insensitive_field_(allow_case_insensitive_field),
        allow_unknown_field_(allow_unknown_field),
        allow_unknown_extension_(allow_unknown_extension),
        allow_unknown_enum_(allow_unknown_enum),
        allow_field_number_(allow_field_number),
        allow_partial_(allow_partial),
        recursion_limit_(recursion_limit),
        had_errors_(false) {
    // For backwards-compatibility with proto1, we need to allow the 'f' suffix
    // for floats.
    tokenizer_.set_allow_f_after_float(true);

    // '#' starts a comment.
    tokenizer_.set_comment_style(io::Tokenizer::SH_COMMENT_STYLE);

    if (allow_relaxed_whitespace) {
      tokenizer_.set_require_space_after_number(false);
      tokenizer_.set_allow_multiline_strings(true);
    }

    // Consume the starting token.
    tokenizer_.Next();
  }
  ~ParserImpl() {}

  // Parses the ASCII representation specified in input and saves the
  // information into the output pointer (a Message). Returns
  // false if an error occurs (an error will also be logged to
  // GOOGLE_LOG(ERROR)).
  bool Parse(Message* output) {
    // Consume fields until we cannot do so anymore.
    while (true) {
      if (LookingAtType(io::Tokenizer::TYPE_END)) {
        return !had_errors_;
      }

      DO(ConsumeField(output));
    }
  }

  bool ParseField(const FieldDescriptor* field, Message* output) {
    bool suc;
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      suc = ConsumeFieldMessage(output, output->GetReflection(), field);
    } else {
      suc = ConsumeFieldValue(output, output->GetReflection(), field);
    }
    return suc && LookingAtType(io::Tokenizer::TYPE_END);
  }

  void ReportError(int line, int col, const std::string& message) {
    had_errors_ = true;
    if (error_collector_ == nullptr) {
      if (line >= 0) {
        GOOGLE_LOG(ERROR) << "Error parsing text-format "
                   << root_message_type_->full_name() << ": " << (line + 1)
                   << ":" << (col + 1) << ": " << message;
      } else {
        GOOGLE_LOG(ERROR) << "Error parsing text-format "
                   << root_message_type_->full_name() << ": " << message;
      }
    } else {
      error_collector_->AddError(line, col, message);
    }
  }

  void ReportWarning(int line, int col, const std::string& message) {
    if (error_collector_ == nullptr) {
      if (line >= 0) {
        GOOGLE_LOG(WARNING) << "Warning parsing text-format "
                     << root_message_type_->full_name() << ": " << (line + 1)
                     << ":" << (col + 1) << ": " << message;
      } else {
        GOOGLE_LOG(WARNING) << "Warning parsing text-format "
                     << root_message_type_->full_name() << ": " << message;
      }
    } else {
      error_collector_->AddWarning(line, col, message);
    }
  }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ParserImpl);

  // Reports an error with the given message with information indicating
  // the position (as derived from the current token).
  void ReportError(const std::string& message) {
    ReportError(tokenizer_.current().line, tokenizer_.current().column,
                message);
  }

  // Reports a warning with the given message with information indicating
  // the position (as derived from the current token).
  void ReportWarning(const std::string& message) {
    ReportWarning(tokenizer_.current().line, tokenizer_.current().column,
                  message);
  }

  // Consumes the specified message with the given starting delimiter.
  // This method checks to see that the end delimiter at the conclusion of
  // the consumption matches the starting delimiter passed in here.
  bool ConsumeMessage(Message* message, const std::string delimiter) {
    while (!LookingAt(">") && !LookingAt("}")) {
      DO(ConsumeField(message));
    }

    // Confirm that we have a valid ending delimiter.
    DO(Consume(delimiter));
    return true;
  }

  // Consume either "<" or "{".
  bool ConsumeMessageDelimiter(std::string* delimiter) {
    if (TryConsume("<")) {
      *delimiter = ">";
    } else {
      DO(Consume("{"));
      *delimiter = "}";
    }
    return true;
  }


  // Consumes the current field (as returned by the tokenizer) on the
  // passed in message.
  bool ConsumeField(Message* message) {
    const Reflection* reflection = message->GetReflection();
    const Descriptor* descriptor = message->GetDescriptor();

    std::string field_name;
    bool reserved_field = false;
    const FieldDescriptor* field = nullptr;
    int start_line = tokenizer_.current().line;
    int start_column = tokenizer_.current().column;

    const FieldDescriptor* any_type_url_field;
    const FieldDescriptor* any_value_field;
    if (internal::GetAnyFieldDescriptors(*message, &any_type_url_field,
                                         &any_value_field) &&
        TryConsume("[")) {
      std::string full_type_name, prefix;
      DO(ConsumeAnyTypeUrl(&full_type_name, &prefix));
      DO(Consume("]"));
      TryConsume(":");  // ':' is optional between message labels and values.
      std::string serialized_value;
      const Descriptor* value_descriptor =
          finder_ ? finder_->FindAnyType(*message, prefix, full_type_name)
                  : DefaultFinderFindAnyType(*message, prefix, full_type_name);
      if (value_descriptor == nullptr) {
        ReportError("Could not find type \"" + prefix + full_type_name +
                    "\" stored in google.protobuf.Any.");
        return false;
      }
      DO(ConsumeAnyValue(value_descriptor, &serialized_value));
      if (singular_overwrite_policy_ == FORBID_SINGULAR_OVERWRITES) {
        // Fail if any_type_url_field has already been specified.
        if ((!any_type_url_field->is_repeated() &&
             reflection->HasField(*message, any_type_url_field)) ||
            (!any_value_field->is_repeated() &&
             reflection->HasField(*message, any_value_field))) {
          ReportError("Non-repeated Any specified multiple times.");
          return false;
        }
      }
      reflection->SetString(message, any_type_url_field,
                            std::string(prefix + full_type_name));
      reflection->SetString(message, any_value_field, serialized_value);
      return true;
    }
    if (TryConsume("[")) {
      // Extension.
      DO(ConsumeFullTypeName(&field_name));
      DO(Consume("]"));

      field = finder_ ? finder_->FindExtension(message, field_name)
                      : DefaultFinderFindExtension(message, field_name);

      if (field == nullptr) {
        if (!allow_unknown_field_ && !allow_unknown_extension_) {
          ReportError("Extension \"" + field_name +
                      "\" is not defined or "
                      "is not an extension of \"" +
                      descriptor->full_name() + "\".");
          return false;
        } else {
          ReportWarning("Ignoring extension \"" + field_name +
                        "\" which is not defined or is not an extension of \"" +
                        descriptor->full_name() + "\".");
        }
      }
    } else {
      DO(ConsumeIdentifier(&field_name));

      int32 field_number;
      if (allow_field_number_ &&
          safe_strto32(field_name, &field_number)) {
        if (descriptor->IsExtensionNumber(field_number)) {
          field = finder_
                      ? finder_->FindExtensionByNumber(descriptor, field_number)
                      : DefaultFinderFindExtensionByNumber(descriptor,
                                                           field_number);
        } else if (descriptor->IsReservedNumber(field_number)) {
          reserved_field = true;
        } else {
          field = descriptor->FindFieldByNumber(field_number);
        }
      } else {
        field = descriptor->FindFieldByName(field_name);
        // Group names are expected to be capitalized as they appear in the
        // .proto file, which actually matches their type names, not their
        // field names.
        if (field == nullptr) {
          std::string lower_field_name = field_name;
          LowerString(&lower_field_name);
          field = descriptor->FindFieldByName(lower_field_name);
          // If the case-insensitive match worked but the field is NOT a group,
          if (field != nullptr &&
              field->type() != FieldDescriptor::TYPE_GROUP) {
            field = nullptr;
          }
        }
        // Again, special-case group names as described above.
        if (field != nullptr && field->type() == FieldDescriptor::TYPE_GROUP &&
            field->message_type()->name() != field_name) {
          field = nullptr;
        }

        if (field == nullptr && allow_case_insensitive_field_) {
          std::string lower_field_name = field_name;
          LowerString(&lower_field_name);
          field = descriptor->FindFieldByLowercaseName(lower_field_name);
        }

        if (field == nullptr) {
          reserved_field = descriptor->IsReservedName(field_name);
        }
      }

      if (field == nullptr && !reserved_field) {
        if (!allow_unknown_field_) {
          ReportError("Message type \"" + descriptor->full_name() +
                      "\" has no field named \"" + field_name + "\".");
          return false;
        } else {
          ReportWarning("Message type \"" + descriptor->full_name() +
                        "\" has no field named \"" + field_name + "\".");
        }
      }
    }

    // Skips unknown or reserved fields.
    if (field == nullptr) {
      GOOGLE_CHECK(allow_unknown_field_ || allow_unknown_extension_ || reserved_field);

      // Try to guess the type of this field.
      // If this field is not a message, there should be a ":" between the
      // field name and the field value and also the field value should not
      // start with "{" or "<" which indicates the beginning of a message body.
      // If there is no ":" or there is a "{" or "<" after ":", this field has
      // to be a message or the input is ill-formed.
      if (TryConsume(":") && !LookingAt("{") && !LookingAt("<")) {
        return SkipFieldValue();
      } else {
        return SkipFieldMessage();
      }
    }

    if (singular_overwrite_policy_ == FORBID_SINGULAR_OVERWRITES) {
      // Fail if the field is not repeated and it has already been specified.
      if (!field->is_repeated() && reflection->HasField(*message, field)) {
        ReportError("Non-repeated field \"" + field_name +
                    "\" is specified multiple times.");
        return false;
      }
      // Fail if the field is a member of a oneof and another member has already
      // been specified.
      const OneofDescriptor* oneof = field->containing_oneof();
      if (oneof != nullptr && reflection->HasOneof(*message, oneof)) {
        const FieldDescriptor* other_field =
            reflection->GetOneofFieldDescriptor(*message, oneof);
        ReportError("Field \"" + field_name +
                    "\" is specified along with "
                    "field \"" +
                    other_field->name() +
                    "\", another member "
                    "of oneof \"" +
                    oneof->name() + "\".");
        return false;
      }
    }

    // Perform special handling for embedded message types.
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      // ':' is optional here.
      bool consumed_semicolon = TryConsume(":");
      if (consumed_semicolon && field->options().weak() &&
          LookingAtType(io::Tokenizer::TYPE_STRING)) {
        // we are getting a bytes string for a weak field.
        std::string tmp;
        DO(ConsumeString(&tmp));
        MessageFactory* factory =
            finder_ ? finder_->FindExtensionFactory(field) : nullptr;
        reflection->MutableMessage(message, field, factory)
            ->ParseFromString(tmp);
        goto label_skip_parsing;
      }
    } else {
      // ':' is required here.
      DO(Consume(":"));
    }

    if (field->is_repeated() && TryConsume("[")) {
      // Short repeated format, e.g.  "foo: [1, 2, 3]".
      if (!TryConsume("]")) {
        // "foo: []" is treated as empty.
        while (true) {
          if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
            // Perform special handling for embedded message types.
            DO(ConsumeFieldMessage(message, reflection, field));
          } else {
            DO(ConsumeFieldValue(message, reflection, field));
          }
          if (TryConsume("]")) {
            break;
          }
          DO(Consume(","));
        }
      }
    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      DO(ConsumeFieldMessage(message, reflection, field));
    } else {
      DO(ConsumeFieldValue(message, reflection, field));
    }
  label_skip_parsing:
    // For historical reasons, fields may optionally be separated by commas or
    // semicolons.
    TryConsume(";") || TryConsume(",");

    if (field->options().deprecated()) {
      ReportWarning("text format contains deprecated field \"" + field_name +
                    "\"");
    }

    // If a parse info tree exists, add the location for the parsed
    // field.
    if (parse_info_tree_ != nullptr) {
      RecordLocation(parse_info_tree_, field,
                     ParseLocation(start_line, start_column));
    }

    return true;
  }

  // Skips the next field including the field's name and value.
  bool SkipField() {
    if (TryConsume("[")) {
      // Extension name or type URL.
      DO(ConsumeTypeUrlOrFullTypeName());
      DO(Consume("]"));
    } else {
      std::string field_name;
      DO(ConsumeIdentifier(&field_name));
    }

    // Try to guess the type of this field.
    // If this field is not a message, there should be a ":" between the
    // field name and the field value and also the field value should not
    // start with "{" or "<" which indicates the beginning of a message body.
    // If there is no ":" or there is a "{" or "<" after ":", this field has
    // to be a message or the input is ill-formed.
    if (TryConsume(":") && !LookingAt("{") && !LookingAt("<")) {
      DO(SkipFieldValue());
    } else {
      DO(SkipFieldMessage());
    }
    // For historical reasons, fields may optionally be separated by commas or
    // semicolons.
    TryConsume(";") || TryConsume(",");
    return true;
  }

  bool ConsumeFieldMessage(Message* message, const Reflection* reflection,
                           const FieldDescriptor* field) {
    if (--recursion_limit_ < 0) {
      ReportError("Message is too deep");
      return false;
    }
    // If the parse information tree is not nullptr, create a nested one
    // for the nested message.
    ParseInfoTree* parent = parse_info_tree_;
    if (parent != nullptr) {
      parse_info_tree_ = CreateNested(parent, field);
    }

    std::string delimiter;
    DO(ConsumeMessageDelimiter(&delimiter));
    MessageFactory* factory =
        finder_ ? finder_->FindExtensionFactory(field) : nullptr;
    if (field->is_repeated()) {
      DO(ConsumeMessage(reflection->AddMessage(message, field, factory),
                        delimiter));
    } else {
      DO(ConsumeMessage(reflection->MutableMessage(message, field, factory),
                        delimiter));
    }

    ++recursion_limit_;

    // Reset the parse information tree.
    parse_info_tree_ = parent;
    return true;
  }

  // Skips the whole body of a message including the beginning delimiter and
  // the ending delimiter.
  bool SkipFieldMessage() {
    std::string delimiter;
    DO(ConsumeMessageDelimiter(&delimiter));
    while (!LookingAt(">") && !LookingAt("}")) {
      DO(SkipField());
    }
    DO(Consume(delimiter));
    return true;
  }

  bool ConsumeFieldValue(Message* message, const Reflection* reflection,
                         const FieldDescriptor* field) {
// Define an easy to use macro for setting fields. This macro checks
// to see if the field is repeated (in which case we need to use the Add
// methods or not (in which case we need to use the Set methods).
#define SET_FIELD(CPPTYPE, VALUE)                    \
  if (field->is_repeated()) {                        \
    reflection->Add##CPPTYPE(message, field, VALUE); \
  } else {                                           \
    reflection->Set##CPPTYPE(message, field, VALUE); \
  }

    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_INT32: {
        int64 value;
        DO(ConsumeSignedInteger(&value, kint32max));
        SET_FIELD(Int32, static_cast<int32>(value));
        break;
      }

      case FieldDescriptor::CPPTYPE_UINT32: {
        uint64 value;
        DO(ConsumeUnsignedInteger(&value, kuint32max));
        SET_FIELD(UInt32, static_cast<uint32>(value));
        break;
      }

      case FieldDescriptor::CPPTYPE_INT64: {
        int64 value;
        DO(ConsumeSignedInteger(&value, kint64max));
        SET_FIELD(Int64, value);
        break;
      }

      case FieldDescriptor::CPPTYPE_UINT64: {
        uint64 value;
        DO(ConsumeUnsignedInteger(&value, kuint64max));
        SET_FIELD(UInt64, value);
        break;
      }

      case FieldDescriptor::CPPTYPE_FLOAT: {
        double value;
        DO(ConsumeDouble(&value));
        SET_FIELD(Float, io::SafeDoubleToFloat(value));
        break;
      }

      case FieldDescriptor::CPPTYPE_DOUBLE: {
        double value;
        DO(ConsumeDouble(&value));
        SET_FIELD(Double, value);
        break;
      }

      case FieldDescriptor::CPPTYPE_STRING: {
        std::string value;
        DO(ConsumeString(&value));
        SET_FIELD(String, value);
        break;
      }

      case FieldDescriptor::CPPTYPE_BOOL: {
        if (LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
          uint64 value;
          DO(ConsumeUnsignedInteger(&value, 1));
          SET_FIELD(Bool, value);
        } else {
          std::string value;
          DO(ConsumeIdentifier(&value));
          if (value == "true" || value == "True" || value == "t") {
            SET_FIELD(Bool, true);
          } else if (value == "false" || value == "False" || value == "f") {
            SET_FIELD(Bool, false);
          } else {
            ReportError("Invalid value for boolean field \"" + field->name() +
                        "\". Value: \"" + value + "\".");
            return false;
          }
        }
        break;
      }

      case FieldDescriptor::CPPTYPE_ENUM: {
        std::string value;
        int64 int_value = kint64max;
        const EnumDescriptor* enum_type = field->enum_type();
        const EnumValueDescriptor* enum_value = nullptr;

        if (LookingAtType(io::Tokenizer::TYPE_IDENTIFIER)) {
          DO(ConsumeIdentifier(&value));
          // Find the enumeration value.
          enum_value = enum_type->FindValueByName(value);

        } else if (LookingAt("-") ||
                   LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
          DO(ConsumeSignedInteger(&int_value, kint32max));
          value = StrCat(int_value);  // for error reporting
          enum_value = enum_type->FindValueByNumber(int_value);
        } else {
          ReportError("Expected integer or identifier, got: " +
                      tokenizer_.current().text);
          return false;
        }

        if (enum_value == nullptr) {
          if (int_value != kint64max &&
              reflection->SupportsUnknownEnumValues()) {
            SET_FIELD(EnumValue, int_value);
            return true;
          } else if (!allow_unknown_enum_) {
            ReportError("Unknown enumeration value of \"" + value +
                        "\" for "
                        "field \"" +
                        field->name() + "\".");
            return false;
          } else {
            ReportWarning("Unknown enumeration value of \"" + value +
                          "\" for "
                          "field \"" +
                          field->name() + "\".");
            return true;
          }
        }

        SET_FIELD(Enum, enum_value);
        break;
      }

      case FieldDescriptor::CPPTYPE_MESSAGE: {
        // We should never get here. Put here instead of a default
        // so that if new types are added, we get a nice compiler warning.
        GOOGLE_LOG(FATAL) << "Reached an unintended state: CPPTYPE_MESSAGE";
        break;
      }
    }
#undef SET_FIELD
    return true;
  }

  bool SkipFieldValue() {
    if (LookingAtType(io::Tokenizer::TYPE_STRING)) {
      while (LookingAtType(io::Tokenizer::TYPE_STRING)) {
        tokenizer_.Next();
      }
      return true;
    }
    if (TryConsume("[")) {
      while (true) {
        if (!LookingAt("{") && !LookingAt("<")) {
          DO(SkipFieldValue());
        } else {
          DO(SkipFieldMessage());
        }
        if (TryConsume("]")) {
          break;
        }
        DO(Consume(","));
      }
      return true;
    }
    // Possible field values other than string:
    //   12345        => TYPE_INTEGER
    //   -12345       => TYPE_SYMBOL + TYPE_INTEGER
    //   1.2345       => TYPE_FLOAT
    //   -1.2345      => TYPE_SYMBOL + TYPE_FLOAT
    //   inf          => TYPE_IDENTIFIER
    //   -inf         => TYPE_SYMBOL + TYPE_IDENTIFIER
    //   TYPE_INTEGER => TYPE_IDENTIFIER
    // Divides them into two group, one with TYPE_SYMBOL
    // and the other without:
    //   Group one:
    //     12345        => TYPE_INTEGER
    //     1.2345       => TYPE_FLOAT
    //     inf          => TYPE_IDENTIFIER
    //     TYPE_INTEGER => TYPE_IDENTIFIER
    //   Group two:
    //     -12345       => TYPE_SYMBOL + TYPE_INTEGER
    //     -1.2345      => TYPE_SYMBOL + TYPE_FLOAT
    //     -inf         => TYPE_SYMBOL + TYPE_IDENTIFIER
    // As we can see, the field value consists of an optional '-' and one of
    // TYPE_INTEGER, TYPE_FLOAT and TYPE_IDENTIFIER.
    bool has_minus = TryConsume("-");
    if (!LookingAtType(io::Tokenizer::TYPE_INTEGER) &&
        !LookingAtType(io::Tokenizer::TYPE_FLOAT) &&
        !LookingAtType(io::Tokenizer::TYPE_IDENTIFIER)) {
      std::string text = tokenizer_.current().text;
      ReportError("Cannot skip field value, unexpected token: " + text);
      return false;
    }
    // Combination of '-' and TYPE_IDENTIFIER may result in an invalid field
    // value while other combinations all generate valid values.
    // We check if the value of this combination is valid here.
    // TYPE_IDENTIFIER after a '-' should be one of the float values listed
    // below:
    //   inf, inff, infinity, nan
    if (has_minus && LookingAtType(io::Tokenizer::TYPE_IDENTIFIER)) {
      std::string text = tokenizer_.current().text;
      LowerString(&text);
      if (text != "inf" &&
          text != "infinity" && text != "nan") {
        ReportError("Invalid float number: " + text);
        return false;
      }
    }
    tokenizer_.Next();
    return true;
  }

  // Returns true if the current token's text is equal to that specified.
  bool LookingAt(const std::string& text) {
    return tokenizer_.current().text == text;
  }

  // Returns true if the current token's type is equal to that specified.
  bool LookingAtType(io::Tokenizer::TokenType token_type) {
    return tokenizer_.current().type == token_type;
  }

  // Consumes an identifier and saves its value in the identifier parameter.
  // Returns false if the token is not of type IDENTFIER.
  bool ConsumeIdentifier(std::string* identifier) {
    if (LookingAtType(io::Tokenizer::TYPE_IDENTIFIER)) {
      *identifier = tokenizer_.current().text;
      tokenizer_.Next();
      return true;
    }

    // If allow_field_numer_ or allow_unknown_field_ is true, we should able
    // to parse integer identifiers.
    if ((allow_field_number_ || allow_unknown_field_ ||
         allow_unknown_extension_) &&
        LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
      *identifier = tokenizer_.current().text;
      tokenizer_.Next();
      return true;
    }

    ReportError("Expected identifier, got: " + tokenizer_.current().text);
    return false;
  }

  // Consume a string of form "<id1>.<id2>....<idN>".
  bool ConsumeFullTypeName(std::string* name) {
    DO(ConsumeIdentifier(name));
    while (TryConsume(".")) {
      std::string part;
      DO(ConsumeIdentifier(&part));
      *name += ".";
      *name += part;
    }
    return true;
  }

  bool ConsumeTypeUrlOrFullTypeName() {
    std::string discarded;
    DO(ConsumeIdentifier(&discarded));
    while (TryConsume(".") || TryConsume("/")) {
      DO(ConsumeIdentifier(&discarded));
    }
    return true;
  }

  // Consumes a string and saves its value in the text parameter.
  // Returns false if the token is not of type STRING.
  bool ConsumeString(std::string* text) {
    if (!LookingAtType(io::Tokenizer::TYPE_STRING)) {
      ReportError("Expected string, got: " + tokenizer_.current().text);
      return false;
    }

    text->clear();
    while (LookingAtType(io::Tokenizer::TYPE_STRING)) {
      io::Tokenizer::ParseStringAppend(tokenizer_.current().text, text);

      tokenizer_.Next();
    }

    return true;
  }

  // Consumes a uint64 and saves its value in the value parameter.
  // Returns false if the token is not of type INTEGER.
  bool ConsumeUnsignedInteger(uint64* value, uint64 max_value) {
    if (!LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
      ReportError("Expected integer, got: " + tokenizer_.current().text);
      return false;
    }

    if (!io::Tokenizer::ParseInteger(tokenizer_.current().text, max_value,
                                     value)) {
      ReportError("Integer out of range (" + tokenizer_.current().text + ")");
      return false;
    }

    tokenizer_.Next();
    return true;
  }

  // Consumes an int64 and saves its value in the value parameter.
  // Note that since the tokenizer does not support negative numbers,
  // we actually may consume an additional token (for the minus sign) in this
  // method. Returns false if the token is not an integer
  // (signed or otherwise).
  bool ConsumeSignedInteger(int64* value, uint64 max_value) {
    bool negative = false;

    if (TryConsume("-")) {
      negative = true;
      // Two's complement always allows one more negative integer than
      // positive.
      ++max_value;
    }

    uint64 unsigned_value;

    DO(ConsumeUnsignedInteger(&unsigned_value, max_value));

    if (negative) {
      if ((static_cast<uint64>(kint64max) + 1) == unsigned_value) {
        *value = kint64min;
      } else {
        *value = -static_cast<int64>(unsigned_value);
      }
    } else {
      *value = static_cast<int64>(unsigned_value);
    }

    return true;
  }

  // Consumes a double and saves its value in the value parameter.
  // Accepts decimal numbers only, rejects hex or oct numbers.
  bool ConsumeUnsignedDecimalAsDouble(double* value, uint64 max_value) {
    if (!LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
      ReportError("Expected integer, got: " + tokenizer_.current().text);
      return false;
    }

    const std::string& text = tokenizer_.current().text;
    if (IsHexNumber(text) || IsOctNumber(text)) {
      ReportError("Expect a decimal number, got: " + text);
      return false;
    }

    uint64 uint64_value;
    if (io::Tokenizer::ParseInteger(text, max_value, &uint64_value)) {
      *value = static_cast<double>(uint64_value);
    } else {
      // Uint64 overflow, attempt to parse as a double instead.
      *value = io::Tokenizer::ParseFloat(text);
    }

    tokenizer_.Next();
    return true;
  }

  // Consumes a double and saves its value in the value parameter.
  // Note that since the tokenizer does not support negative numbers,
  // we actually may consume an additional token (for the minus sign) in this
  // method. Returns false if the token is not a double
  // (signed or otherwise).
  bool ConsumeDouble(double* value) {
    bool negative = false;

    if (TryConsume("-")) {
      negative = true;
    }

    // A double can actually be an integer, according to the tokenizer.
    // Therefore, we must check both cases here.
    if (LookingAtType(io::Tokenizer::TYPE_INTEGER)) {
      // We have found an integer value for the double.
      DO(ConsumeUnsignedDecimalAsDouble(value, kuint64max));
    } else if (LookingAtType(io::Tokenizer::TYPE_FLOAT)) {
      // We have found a float value for the double.
      *value = io::Tokenizer::ParseFloat(tokenizer_.current().text);

      // Mark the current token as consumed.
      tokenizer_.Next();
    } else if (LookingAtType(io::Tokenizer::TYPE_IDENTIFIER)) {
      std::string text = tokenizer_.current().text;
      LowerString(&text);
      if (text == "inf" ||
          text == "infinity") {
        *value = std::numeric_limits<double>::infinity();
        tokenizer_.Next();
      } else if (text == "nan") {
        *value = std::numeric_limits<double>::quiet_NaN();
        tokenizer_.Next();
      } else {
        ReportError("Expected double, got: " + text);
        return false;
      }
    } else {
      ReportError("Expected double, got: " + tokenizer_.current().text);
      return false;
    }

    if (negative) {
      *value = -*value;
    }

    return true;
  }

  // Consumes Any::type_url value, of form "type.googleapis.com/full.type.Name"
  // or "type.googleprod.com/full.type.Name"
  bool ConsumeAnyTypeUrl(std::string* full_type_name, std::string* prefix) {
    // TODO(saito) Extend Consume() to consume multiple tokens at once, so that
    // this code can be written as just DO(Consume(kGoogleApisTypePrefix)).
    DO(ConsumeIdentifier(prefix));
    while (TryConsume(".")) {
      std::string url;
      DO(ConsumeIdentifier(&url));
      *prefix += "." + url;
    }
    DO(Consume("/"));
    *prefix += "/";
    DO(ConsumeFullTypeName(full_type_name));

    return true;
  }

  // A helper function for reconstructing Any::value. Consumes a text of
  // full_type_name, then serializes it into serialized_value.
  bool ConsumeAnyValue(const Descriptor* value_descriptor,
                       std::string* serialized_value) {
    DynamicMessageFactory factory;
    const Message* value_prototype = factory.GetPrototype(value_descriptor);
    if (value_prototype == nullptr) {
      return false;
    }
    std::unique_ptr<Message> value(value_prototype->New());
    std::string sub_delimiter;
    DO(ConsumeMessageDelimiter(&sub_delimiter));
    DO(ConsumeMessage(value.get(), sub_delimiter));

    if (allow_partial_) {
      value->AppendPartialToString(serialized_value);
    } else {
      if (!value->IsInitialized()) {
        ReportError(
            "Value of type \"" + value_descriptor->full_name() +
            "\" stored in google.protobuf.Any has missing required fields");
        return false;
      }
      value->AppendToString(serialized_value);
    }
    return true;
  }

  // Consumes a token and confirms that it matches that specified in the
  // value parameter. Returns false if the token found does not match that
  // which was specified.
  bool Consume(const std::string& value) {
    const std::string& current_value = tokenizer_.current().text;

    if (current_value != value) {
      ReportError("Expected \"" + value + "\", found \"" + current_value +
                  "\".");
      return false;
    }

    tokenizer_.Next();

    return true;
  }

  // Attempts to consume the supplied value. Returns false if a the
  // token found does not match the value specified.
  bool TryConsume(const std::string& value) {
    if (tokenizer_.current().text == value) {
      tokenizer_.Next();
      return true;
    } else {
      return false;
    }
  }

  // An internal instance of the Tokenizer's error collector, used to
  // collect any base-level parse errors and feed them to the ParserImpl.
  class ParserErrorCollector : public io::ErrorCollector {
   public:
    explicit ParserErrorCollector(TextFormat::Parser::ParserImpl* parser)
        : parser_(parser) {}

    ~ParserErrorCollector() override {}

    void AddError(int line, int column, const std::string& message) override {
      parser_->ReportError(line, column, message);
    }

    void AddWarning(int line, int column, const std::string& message) override {
      parser_->ReportWarning(line, column, message);
    }

   private:
    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ParserErrorCollector);
    TextFormat::Parser::ParserImpl* parser_;
  };

  io::ErrorCollector* error_collector_;
  const TextFormat::Finder* finder_;
  ParseInfoTree* parse_info_tree_;
  ParserErrorCollector tokenizer_error_collector_;
  io::Tokenizer tokenizer_;
  const Descriptor* root_message_type_;
  SingularOverwritePolicy singular_overwrite_policy_;
  const bool allow_case_insensitive_field_;
  const bool allow_unknown_field_;
  const bool allow_unknown_extension_;
  const bool allow_unknown_enum_;
  const bool allow_field_number_;
  const bool allow_partial_;
  int recursion_limit_;
  bool had_errors_;
};

// ===========================================================================
// Internal class for writing text to the io::ZeroCopyOutputStream. Adapted
// from the Printer found in //net/proto2/io/public/printer.h
class TextFormat::Printer::TextGenerator
    : public TextFormat::BaseTextGenerator {
 public:
  explicit TextGenerator(io::ZeroCopyOutputStream* output,
                         int initial_indent_level)
      : output_(output),
        buffer_(nullptr),
        buffer_size_(0),
        at_start_of_line_(true),
        failed_(false),
        indent_level_(initial_indent_level),
        initial_indent_level_(initial_indent_level) {}

  ~TextGenerator() {
    // Only BackUp() if we're sure we've successfully called Next() at least
    // once.
    if (!failed_ && buffer_size_ > 0) {
      output_->BackUp(buffer_size_);
    }
  }

  // Indent text by two spaces.  After calling Indent(), two spaces will be
  // inserted at the beginning of each line of text.  Indent() may be called
  // multiple times to produce deeper indents.
  void Indent() override { ++indent_level_; }

  // Reduces the current indent level by two spaces, or crashes if the indent
  // level is zero.
  void Outdent() override {
    if (indent_level_ == 0 || indent_level_ < initial_indent_level_) {
      GOOGLE_LOG(DFATAL) << " Outdent() without matching Indent().";
      return;
    }

    --indent_level_;
  }

  // Print text to the output stream.
  void Print(const char* text, size_t size) override {
    if (indent_level_ > 0) {
      size_t pos = 0;  // The number of bytes we've written so far.
      for (size_t i = 0; i < size; i++) {
        if (text[i] == '\n') {
          // Saw newline.  If there is more text, we may need to insert an
          // indent here.  So, write what we have so far, including the '\n'.
          Write(text + pos, i - pos + 1);
          pos = i + 1;

          // Setting this true will cause the next Write() to insert an indent
          // first.
          at_start_of_line_ = true;
        }
      }
      // Write the rest.
      Write(text + pos, size - pos);
    } else {
      Write(text, size);
      if (size > 0 && text[size - 1] == '\n') {
        at_start_of_line_ = true;
      }
    }
  }

  // True if any write to the underlying stream failed.  (We don't just
  // crash in this case because this is an I/O failure, not a programming
  // error.)
  bool failed() const { return failed_; }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TextGenerator);

  void Write(const char* data, size_t size) {
    if (failed_) return;
    if (size == 0) return;

    if (at_start_of_line_) {
      // Insert an indent.
      at_start_of_line_ = false;
      WriteIndent();
      if (failed_) return;
    }

    while (size > buffer_size_) {
      // Data exceeds space in the buffer.  Copy what we can and request a
      // new buffer.
      if (buffer_size_ > 0) {
        memcpy(buffer_, data, buffer_size_);
        data += buffer_size_;
        size -= buffer_size_;
      }
      void* void_buffer = nullptr;
      failed_ = !output_->Next(&void_buffer, &buffer_size_);
      if (failed_) return;
      buffer_ = reinterpret_cast<char*>(void_buffer);
    }

    // Buffer is big enough to receive the data; copy it.
    memcpy(buffer_, data, size);
    buffer_ += size;
    buffer_size_ -= size;
  }

  void WriteIndent() {
    if (indent_level_ == 0) {
      return;
    }
    GOOGLE_DCHECK(!failed_);
    int size = 2 * indent_level_;

    while (size > buffer_size_) {
      // Data exceeds space in the buffer. Write what we can and request a new
      // buffer.
      if (buffer_size_ > 0) {
        memset(buffer_, ' ', buffer_size_);
      }
      size -= buffer_size_;
      void* void_buffer;
      failed_ = !output_->Next(&void_buffer, &buffer_size_);
      if (failed_) return;
      buffer_ = reinterpret_cast<char*>(void_buffer);
    }

    // Buffer is big enough to receive the data; copy it.
    memset(buffer_, ' ', size);
    buffer_ += size;
    buffer_size_ -= size;
  }

  io::ZeroCopyOutputStream* const output_;
  char* buffer_;
  int buffer_size_;
  bool at_start_of_line_;
  bool failed_;

  int indent_level_;
  int initial_indent_level_;
};

// ===========================================================================
// Implementation of the default Finder for extensions.
TextFormat::Finder::~Finder() {}

const FieldDescriptor* TextFormat::Finder::FindExtension(
    Message* message, const std::string& name) const {
  return DefaultFinderFindExtension(message, name);
}

const FieldDescriptor* TextFormat::Finder::FindExtensionByNumber(
    const Descriptor* descriptor, int number) const {
  return DefaultFinderFindExtensionByNumber(descriptor, number);
}

const Descriptor* TextFormat::Finder::FindAnyType(
    const Message& message, const std::string& prefix,
    const std::string& name) const {
  return DefaultFinderFindAnyType(message, prefix, name);
}

MessageFactory* TextFormat::Finder::FindExtensionFactory(
    const FieldDescriptor* field) const {
  return nullptr;
}

// ===========================================================================

TextFormat::Parser::Parser()
    : error_collector_(nullptr),
      finder_(nullptr),
      parse_info_tree_(nullptr),
      allow_partial_(false),
      allow_case_insensitive_field_(false),
      allow_unknown_field_(false),
      allow_unknown_extension_(false),
      allow_unknown_enum_(false),
      allow_field_number_(false),
      allow_relaxed_whitespace_(false),
      allow_singular_overwrites_(false),
      recursion_limit_(std::numeric_limits<int>::max()) {}

TextFormat::Parser::~Parser() {}

namespace {

bool CheckParseInputSize(StringPiece input,
                         io::ErrorCollector* error_collector) {
  if (input.size() > INT_MAX) {
    error_collector->AddError(
        -1, 0,
        StrCat("Input size too large: ", static_cast<int64>(input.size()),
                     " bytes", " > ", INT_MAX, " bytes."));
    return false;
  }
  return true;
}

}  // namespace

bool TextFormat::Parser::Parse(io::ZeroCopyInputStream* input,
                               Message* output) {
  output->Clear();

  ParserImpl::SingularOverwritePolicy overwrites_policy =
      allow_singular_overwrites_ ? ParserImpl::ALLOW_SINGULAR_OVERWRITES
                                 : ParserImpl::FORBID_SINGULAR_OVERWRITES;

  ParserImpl parser(output->GetDescriptor(), input, error_collector_, finder_,
                    parse_info_tree_, overwrites_policy,
                    allow_case_insensitive_field_, allow_unknown_field_,
                    allow_unknown_extension_, allow_unknown_enum_,
                    allow_field_number_, allow_relaxed_whitespace_,
                    allow_partial_, recursion_limit_);
  return MergeUsingImpl(input, output, &parser);
}

bool TextFormat::Parser::ParseFromString(const std::string& input,
                                         Message* output) {
  DO(CheckParseInputSize(input, error_collector_));
  io::ArrayInputStream input_stream(input.data(), input.size());
  return Parse(&input_stream, output);
}


bool TextFormat::Parser::Merge(io::ZeroCopyInputStream* input,
                               Message* output) {
  ParserImpl parser(output->GetDescriptor(), input, error_collector_, finder_,
                    parse_info_tree_, ParserImpl::ALLOW_SINGULAR_OVERWRITES,
                    allow_case_insensitive_field_, allow_unknown_field_,
                    allow_unknown_extension_, allow_unknown_enum_,
                    allow_field_number_, allow_relaxed_whitespace_,
                    allow_partial_, recursion_limit_);
  return MergeUsingImpl(input, output, &parser);
}

bool TextFormat::Parser::MergeFromString(const std::string& input,
                                         Message* output) {
  DO(CheckParseInputSize(input, error_collector_));
  io::ArrayInputStream input_stream(input.data(), input.size());
  return Merge(&input_stream, output);
}


bool TextFormat::Parser::MergeUsingImpl(io::ZeroCopyInputStream* /* input */,
                                        Message* output,
                                        ParserImpl* parser_impl) {
  if (!parser_impl->Parse(output)) return false;
  if (!allow_partial_ && !output->IsInitialized()) {
    std::vector<std::string> missing_fields;
    output->FindInitializationErrors(&missing_fields);
    parser_impl->ReportError(-1, 0,
                             "Message missing required fields: " +
                                 Join(missing_fields, ", "));
    return false;
  }
  return true;
}

bool TextFormat::Parser::ParseFieldValueFromString(const std::string& input,
                                                   const FieldDescriptor* field,
                                                   Message* output) {
  io::ArrayInputStream input_stream(input.data(), input.size());
  ParserImpl parser(
      output->GetDescriptor(), &input_stream, error_collector_, finder_,
      parse_info_tree_, ParserImpl::ALLOW_SINGULAR_OVERWRITES,
      allow_case_insensitive_field_, allow_unknown_field_,
      allow_unknown_extension_, allow_unknown_enum_, allow_field_number_,
      allow_relaxed_whitespace_, allow_partial_, recursion_limit_);
  return parser.ParseField(field, output);
}

/* static */ bool TextFormat::Parse(io::ZeroCopyInputStream* input,
                                    Message* output) {
  return Parser().Parse(input, output);
}

/* static */ bool TextFormat::Merge(io::ZeroCopyInputStream* input,
                                    Message* output) {
  return Parser().Merge(input, output);
}

/* static */ bool TextFormat::ParseFromString(const std::string& input,
                                              Message* output) {
  return Parser().ParseFromString(input, output);
}

/* static */ bool TextFormat::MergeFromString(const std::string& input,
                                              Message* output) {
  return Parser().MergeFromString(input, output);
}


#undef DO

// ===========================================================================

TextFormat::BaseTextGenerator::~BaseTextGenerator() {}

namespace {

// A BaseTextGenerator that writes to a string.
class StringBaseTextGenerator : public TextFormat::BaseTextGenerator {
 public:
  void Print(const char* text, size_t size) override {
    output_.append(text, size);
  }

// Some compilers do not support ref-qualifiers even in C++11 mode.
// Disable the optimization for now and revisit it later.
#if 0   // LANG_CXX11
  std::string Consume() && { return std::move(output_); }
#else   // !LANG_CXX11
  const std::string& Get() { return output_; }
#endif  // LANG_CXX11

 private:
  std::string output_;
};

}  // namespace

// The default implementation for FieldValuePrinter. We just delegate the
// implementation to the default FastFieldValuePrinter to avoid duplicating the
// logic.
TextFormat::FieldValuePrinter::FieldValuePrinter() {}
TextFormat::FieldValuePrinter::~FieldValuePrinter() {}

#if 0  // LANG_CXX11
#define FORWARD_IMPL(fn, ...)            \
  StringBaseTextGenerator generator;     \
  delegate_.fn(__VA_ARGS__, &generator); \
  return std::move(generator).Consume()
#else  // !LANG_CXX11
#define FORWARD_IMPL(fn, ...)            \
  StringBaseTextGenerator generator;     \
  delegate_.fn(__VA_ARGS__, &generator); \
  return generator.Get()
#endif  // LANG_CXX11

std::string TextFormat::FieldValuePrinter::PrintBool(bool val) const {
  FORWARD_IMPL(PrintBool, val);
}
std::string TextFormat::FieldValuePrinter::PrintInt32(int32 val) const {
  FORWARD_IMPL(PrintInt32, val);
}
std::string TextFormat::FieldValuePrinter::PrintUInt32(uint32 val) const {
  FORWARD_IMPL(PrintUInt32, val);
}
std::string TextFormat::FieldValuePrinter::PrintInt64(int64 val) const {
  FORWARD_IMPL(PrintInt64, val);
}
std::string TextFormat::FieldValuePrinter::PrintUInt64(uint64 val) const {
  FORWARD_IMPL(PrintUInt64, val);
}
std::string TextFormat::FieldValuePrinter::PrintFloat(float val) const {
  FORWARD_IMPL(PrintFloat, val);
}
std::string TextFormat::FieldValuePrinter::PrintDouble(double val) const {
  FORWARD_IMPL(PrintDouble, val);
}
std::string TextFormat::FieldValuePrinter::PrintString(
    const std::string& val) const {
  FORWARD_IMPL(PrintString, val);
}
std::string TextFormat::FieldValuePrinter::PrintBytes(
    const std::string& val) const {
  return PrintString(val);
}
std::string TextFormat::FieldValuePrinter::PrintEnum(
    int32 val, const std::string& name) const {
  FORWARD_IMPL(PrintEnum, val, name);
}
std::string TextFormat::FieldValuePrinter::PrintFieldName(
    const Message& message, const Reflection* reflection,
    const FieldDescriptor* field) const {
  FORWARD_IMPL(PrintFieldName, message, reflection, field);
}
std::string TextFormat::FieldValuePrinter::PrintMessageStart(
    const Message& message, int field_index, int field_count,
    bool single_line_mode) const {
  FORWARD_IMPL(PrintMessageStart, message, field_index, field_count,
               single_line_mode);
}
std::string TextFormat::FieldValuePrinter::PrintMessageEnd(
    const Message& message, int field_index, int field_count,
    bool single_line_mode) const {
  FORWARD_IMPL(PrintMessageEnd, message, field_index, field_count,
               single_line_mode);
}
#undef FORWARD_IMPL

TextFormat::FastFieldValuePrinter::FastFieldValuePrinter() {}
TextFormat::FastFieldValuePrinter::~FastFieldValuePrinter() {}
void TextFormat::FastFieldValuePrinter::PrintBool(
    bool val, BaseTextGenerator* generator) const {
  if (val) {
    generator->PrintLiteral("true");
  } else {
    generator->PrintLiteral("false");
  }
}
void TextFormat::FastFieldValuePrinter::PrintInt32(
    int32 val, BaseTextGenerator* generator) const {
  generator->PrintString(StrCat(val));
}
void TextFormat::FastFieldValuePrinter::PrintUInt32(
    uint32 val, BaseTextGenerator* generator) const {
  generator->PrintString(StrCat(val));
}
void TextFormat::FastFieldValuePrinter::PrintInt64(
    int64 val, BaseTextGenerator* generator) const {
  generator->PrintString(StrCat(val));
}
void TextFormat::FastFieldValuePrinter::PrintUInt64(
    uint64 val, BaseTextGenerator* generator) const {
  generator->PrintString(StrCat(val));
}
void TextFormat::FastFieldValuePrinter::PrintFloat(
    float val, BaseTextGenerator* generator) const {
  generator->PrintString(SimpleFtoa(val));
}
void TextFormat::FastFieldValuePrinter::PrintDouble(
    double val, BaseTextGenerator* generator) const {
  generator->PrintString(SimpleDtoa(val));
}
void TextFormat::FastFieldValuePrinter::PrintEnum(
    int32 val, const std::string& name, BaseTextGenerator* generator) const {
  generator->PrintString(name);
}

void TextFormat::FastFieldValuePrinter::PrintString(
    const std::string& val, BaseTextGenerator* generator) const {
  generator->PrintLiteral("\"");
  generator->PrintString(CEscape(val));
  generator->PrintLiteral("\"");
}
void TextFormat::FastFieldValuePrinter::PrintBytes(
    const std::string& val, BaseTextGenerator* generator) const {
  PrintString(val, generator);
}
void TextFormat::FastFieldValuePrinter::PrintFieldName(
    const Message& message, int field_index, int field_count,
    const Reflection* reflection, const FieldDescriptor* field,
    BaseTextGenerator* generator) const {
  PrintFieldName(message, reflection, field, generator);
}
void TextFormat::FastFieldValuePrinter::PrintFieldName(
    const Message& message, const Reflection* reflection,
    const FieldDescriptor* field, BaseTextGenerator* generator) const {
  if (field->is_extension()) {
    generator->PrintLiteral("[");
    generator->PrintString(field->PrintableNameForExtension());
    generator->PrintLiteral("]");
  } else if (field->type() == FieldDescriptor::TYPE_GROUP) {
    // Groups must be serialized with their original capitalization.
    generator->PrintString(field->message_type()->name());
  } else {
    generator->PrintString(field->name());
  }
}
void TextFormat::FastFieldValuePrinter::PrintMessageStart(
    const Message& message, int field_index, int field_count,
    bool single_line_mode, BaseTextGenerator* generator) const {
  if (single_line_mode) {
    generator->PrintLiteral(" { ");
  } else {
    generator->PrintLiteral(" {\n");
  }
}
void TextFormat::FastFieldValuePrinter::PrintMessageEnd(
    const Message& message, int field_index, int field_count,
    bool single_line_mode, BaseTextGenerator* generator) const {
  if (single_line_mode) {
    generator->PrintLiteral("} ");
  } else {
    generator->PrintLiteral("}\n");
  }
}

namespace {

// A legacy compatibility wrapper. Takes ownership of the delegate.
class FieldValuePrinterWrapper : public TextFormat::FastFieldValuePrinter {
 public:
  explicit FieldValuePrinterWrapper(
      const TextFormat::FieldValuePrinter* delegate)
      : delegate_(delegate) {}

  void SetDelegate(const TextFormat::FieldValuePrinter* delegate) {
    delegate_.reset(delegate);
  }

  void PrintBool(bool val,
                 TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintBool(val));
  }
  void PrintInt32(int32 val,
                  TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintInt32(val));
  }
  void PrintUInt32(uint32 val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintUInt32(val));
  }
  void PrintInt64(int64 val,
                  TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintInt64(val));
  }
  void PrintUInt64(uint64 val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintUInt64(val));
  }
  void PrintFloat(float val,
                  TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintFloat(val));
  }
  void PrintDouble(double val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintDouble(val));
  }
  void PrintString(const std::string& val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintString(val));
  }
  void PrintBytes(const std::string& val,
                  TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintBytes(val));
  }
  void PrintEnum(int32 val, const std::string& name,
                 TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintEnum(val, name));
  }
  void PrintFieldName(const Message& message, int field_index, int field_count,
                      const Reflection* reflection,
                      const FieldDescriptor* field,
                      TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(
        delegate_->PrintFieldName(message, reflection, field));
  }
  void PrintFieldName(const Message& message, const Reflection* reflection,
                      const FieldDescriptor* field,
                      TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(
        delegate_->PrintFieldName(message, reflection, field));
  }
  void PrintMessageStart(
      const Message& message, int field_index, int field_count,
      bool single_line_mode,
      TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintMessageStart(
        message, field_index, field_count, single_line_mode));
  }
  void PrintMessageEnd(
      const Message& message, int field_index, int field_count,
      bool single_line_mode,
      TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintString(delegate_->PrintMessageEnd(
        message, field_index, field_count, single_line_mode));
  }

 private:
  std::unique_ptr<const TextFormat::FieldValuePrinter> delegate_;
};

// Our own specialization: for UTF8 escaped strings.
class FastFieldValuePrinterUtf8Escaping
    : public TextFormat::FastFieldValuePrinter {
 public:
  void PrintString(const std::string& val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintLiteral("\"");
    generator->PrintString(strings::Utf8SafeCEscape(val));
    generator->PrintLiteral("\"");
  }
  void PrintBytes(const std::string& val,
                  TextFormat::BaseTextGenerator* generator) const override {
    return FastFieldValuePrinter::PrintString(val, generator);
  }
};

}  // namespace

TextFormat::Printer::Printer()
    : initial_indent_level_(0),
      single_line_mode_(false),
      use_field_number_(false),
      use_short_repeated_primitives_(false),
      hide_unknown_fields_(false),
      print_message_fields_in_index_order_(false),
      expand_any_(false),
      truncate_string_field_longer_than_(0LL),
      finder_(nullptr) {
  SetUseUtf8StringEscaping(false);
}

TextFormat::Printer::~Printer() {
  STLDeleteValues(&custom_printers_);
  STLDeleteValues(&custom_message_printers_);
}

void TextFormat::Printer::SetUseUtf8StringEscaping(bool as_utf8) {
  SetDefaultFieldValuePrinter(as_utf8 ? new FastFieldValuePrinterUtf8Escaping()
                                      : new FastFieldValuePrinter());
}

void TextFormat::Printer::SetDefaultFieldValuePrinter(
    const FieldValuePrinter* printer) {
  default_field_value_printer_.reset(new FieldValuePrinterWrapper(printer));
}

void TextFormat::Printer::SetDefaultFieldValuePrinter(
    const FastFieldValuePrinter* printer) {
  default_field_value_printer_.reset(printer);
}

bool TextFormat::Printer::RegisterFieldValuePrinter(
    const FieldDescriptor* field, const FieldValuePrinter* printer) {
  if (field == nullptr || printer == nullptr) {
    return false;
  }
  FieldValuePrinterWrapper* const wrapper =
      new FieldValuePrinterWrapper(nullptr);
  if (custom_printers_.insert(std::make_pair(field, wrapper)).second) {
    wrapper->SetDelegate(printer);
    return true;
  } else {
    delete wrapper;
    return false;
  }
}

bool TextFormat::Printer::RegisterFieldValuePrinter(
    const FieldDescriptor* field, const FastFieldValuePrinter* printer) {
  return field != nullptr && printer != nullptr &&
         custom_printers_.insert(std::make_pair(field, printer)).second;
}

bool TextFormat::Printer::RegisterMessagePrinter(
    const Descriptor* descriptor, const MessagePrinter* printer) {
  return descriptor != nullptr && printer != nullptr &&
         custom_message_printers_.insert(std::make_pair(descriptor, printer))
             .second;
}

bool TextFormat::Printer::PrintToString(const Message& message,
                                        std::string* output) const {
  GOOGLE_DCHECK(output) << "output specified is nullptr";

  output->clear();
  io::StringOutputStream output_stream(output);

  return Print(message, &output_stream);
}

bool TextFormat::Printer::PrintUnknownFieldsToString(
    const UnknownFieldSet& unknown_fields, std::string* output) const {
  GOOGLE_DCHECK(output) << "output specified is nullptr";

  output->clear();
  io::StringOutputStream output_stream(output);
  return PrintUnknownFields(unknown_fields, &output_stream);
}

bool TextFormat::Printer::Print(const Message& message,
                                io::ZeroCopyOutputStream* output) const {
  TextGenerator generator(output, initial_indent_level_);

  Print(message, &generator);

  // Output false if the generator failed internally.
  return !generator.failed();
}

bool TextFormat::Printer::PrintUnknownFields(
    const UnknownFieldSet& unknown_fields,
    io::ZeroCopyOutputStream* output) const {
  TextGenerator generator(output, initial_indent_level_);

  PrintUnknownFields(unknown_fields, &generator);

  // Output false if the generator failed internally.
  return !generator.failed();
}

namespace {
// Comparison functor for sorting FieldDescriptors by field index.
// Normal fields have higher precedence than extensions.
struct FieldIndexSorter {
  bool operator()(const FieldDescriptor* left,
                  const FieldDescriptor* right) const {
    if (left->is_extension() && right->is_extension()) {
      return left->number() < right->number();
    } else if (left->is_extension()) {
      return false;
    } else if (right->is_extension()) {
      return true;
    } else {
      return left->index() < right->index();
    }
  }
};

}  // namespace

bool TextFormat::Printer::PrintAny(const Message& message,
                                   TextGenerator* generator) const {
  const FieldDescriptor* type_url_field;
  const FieldDescriptor* value_field;
  if (!internal::GetAnyFieldDescriptors(message, &type_url_field,
                                        &value_field)) {
    return false;
  }

  const Reflection* reflection = message.GetReflection();

  // Extract the full type name from the type_url field.
  const std::string& type_url = reflection->GetString(message, type_url_field);
  std::string url_prefix;
  std::string full_type_name;
  if (!internal::ParseAnyTypeUrl(type_url, &url_prefix, &full_type_name)) {
    return false;
  }

  // Print the "value" in text.
  const Descriptor* value_descriptor =
      finder_ ? finder_->FindAnyType(message, url_prefix, full_type_name)
              : DefaultFinderFindAnyType(message, url_prefix, full_type_name);
  if (value_descriptor == nullptr) {
    GOOGLE_LOG(WARNING) << "Proto type " << type_url << " not found";
    return false;
  }
  DynamicMessageFactory factory;
  std::unique_ptr<Message> value_message(
      factory.GetPrototype(value_descriptor)->New());
  std::string serialized_value = reflection->GetString(message, value_field);
  if (!value_message->ParseFromString(serialized_value)) {
    GOOGLE_LOG(WARNING) << type_url << ": failed to parse contents";
    return false;
  }
  generator->PrintLiteral("[");
  generator->PrintString(type_url);
  generator->PrintLiteral("]");
  const FastFieldValuePrinter* printer = FindWithDefault(
      custom_printers_, value_field, default_field_value_printer_.get());
  printer->PrintMessageStart(message, -1, 0, single_line_mode_, generator);
  generator->Indent();
  Print(*value_message, generator);
  generator->Outdent();
  printer->PrintMessageEnd(message, -1, 0, single_line_mode_, generator);
  return true;
}

void TextFormat::Printer::Print(const Message& message,
                                TextGenerator* generator) const {
  const Reflection* reflection = message.GetReflection();
  if (!reflection) {
    // This message does not provide any way to describe its structure.
    // Parse it again in an UnknownFieldSet, and display this instead.
    UnknownFieldSet unknown_fields;
    {
      std::string serialized = message.SerializeAsString();
      io::ArrayInputStream input(serialized.data(), serialized.size());
      unknown_fields.ParseFromZeroCopyStream(&input);
    }
    PrintUnknownFields(unknown_fields, generator);
    return;
  }
  const Descriptor* descriptor = message.GetDescriptor();
  auto itr = custom_message_printers_.find(descriptor);
  if (itr != custom_message_printers_.end()) {
    itr->second->Print(message, single_line_mode_, generator);
    return;
  }
  if (descriptor->full_name() == internal::kAnyFullTypeName && expand_any_ &&
      PrintAny(message, generator)) {
    return;
  }
  std::vector<const FieldDescriptor*> fields;
  if (descriptor->options().map_entry()) {
    fields.push_back(descriptor->field(0));
    fields.push_back(descriptor->field(1));
  } else {
    reflection->ListFields(message, &fields);
  }

  if (print_message_fields_in_index_order_) {
    std::sort(fields.begin(), fields.end(), FieldIndexSorter());
  }
  for (int i = 0; i < fields.size(); i++) {
    PrintField(message, reflection, fields[i], generator);
  }
  if (!hide_unknown_fields_) {
    PrintUnknownFields(reflection->GetUnknownFields(message), generator);
  }
}

void TextFormat::Printer::PrintFieldValueToString(const Message& message,
                                                  const FieldDescriptor* field,
                                                  int index,
                                                  std::string* output) const {
  GOOGLE_DCHECK(output) << "output specified is nullptr";

  output->clear();
  io::StringOutputStream output_stream(output);
  TextGenerator generator(&output_stream, initial_indent_level_);

  PrintFieldValue(message, message.GetReflection(), field, index, &generator);
}

class MapEntryMessageComparator {
 public:
  explicit MapEntryMessageComparator(const Descriptor* descriptor)
      : field_(descriptor->field(0)) {}

  bool operator()(const Message* a, const Message* b) {
    const Reflection* reflection = a->GetReflection();
    switch (field_->cpp_type()) {
      case FieldDescriptor::CPPTYPE_BOOL: {
        bool first = reflection->GetBool(*a, field_);
        bool second = reflection->GetBool(*b, field_);
        return first < second;
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        int32 first = reflection->GetInt32(*a, field_);
        int32 second = reflection->GetInt32(*b, field_);
        return first < second;
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        int64 first = reflection->GetInt64(*a, field_);
        int64 second = reflection->GetInt64(*b, field_);
        return first < second;
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        uint32 first = reflection->GetUInt32(*a, field_);
        uint32 second = reflection->GetUInt32(*b, field_);
        return first < second;
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        uint64 first = reflection->GetUInt64(*a, field_);
        uint64 second = reflection->GetUInt64(*b, field_);
        return first < second;
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        std::string first = reflection->GetString(*a, field_);
        std::string second = reflection->GetString(*b, field_);
        return first < second;
      }
      default:
        GOOGLE_LOG(DFATAL) << "Invalid key for map field.";
        return true;
    }
  }

 private:
  const FieldDescriptor* field_;
};

namespace internal {
class MapFieldPrinterHelper {
 public:
  // DynamicMapSorter::Sort cannot be used because it enfores syncing with
  // repeated field.
  static bool SortMap(const Message& message, const Reflection* reflection,
                      const FieldDescriptor* field, MessageFactory* factory,
                      std::vector<const Message*>* sorted_map_field);
  static void CopyKey(const MapKey& key, Message* message,
                      const FieldDescriptor* field_desc);
  static void CopyValue(const MapValueRef& value, Message* message,
                        const FieldDescriptor* field_desc);
};

// Returns true if elements contained in sorted_map_field need to be released.
bool MapFieldPrinterHelper::SortMap(
    const Message& message, const Reflection* reflection,
    const FieldDescriptor* field, MessageFactory* factory,
    std::vector<const Message*>* sorted_map_field) {
  bool need_release = false;
  const MapFieldBase& base = *reflection->GetMapData(message, field);

  if (base.IsRepeatedFieldValid()) {
    const RepeatedPtrField<Message>& map_field =
        reflection->GetRepeatedPtrField<Message>(message, field);
    for (int i = 0; i < map_field.size(); ++i) {
      sorted_map_field->push_back(
          const_cast<RepeatedPtrField<Message>*>(&map_field)->Mutable(i));
    }
  } else {
    // TODO(teboring): For performance, instead of creating map entry message
    // for each element, just store map keys and sort them.
    const Descriptor* map_entry_desc = field->message_type();
    const Message* prototype = factory->GetPrototype(map_entry_desc);
    for (MapIterator iter =
             reflection->MapBegin(const_cast<Message*>(&message), field);
         iter != reflection->MapEnd(const_cast<Message*>(&message), field);
         ++iter) {
      Message* map_entry_message = prototype->New();
      CopyKey(iter.GetKey(), map_entry_message, map_entry_desc->field(0));
      CopyValue(iter.GetValueRef(), map_entry_message,
                map_entry_desc->field(1));
      sorted_map_field->push_back(map_entry_message);
    }
    need_release = true;
  }

  MapEntryMessageComparator comparator(field->message_type());
  std::stable_sort(sorted_map_field->begin(), sorted_map_field->end(),
                   comparator);
  return need_release;
}

void MapFieldPrinterHelper::CopyKey(const MapKey& key, Message* message,
                                    const FieldDescriptor* field_desc) {
  const Reflection* reflection = message->GetReflection();
  switch (field_desc->cpp_type()) {
    case FieldDescriptor::CPPTYPE_DOUBLE:
    case FieldDescriptor::CPPTYPE_FLOAT:
    case FieldDescriptor::CPPTYPE_ENUM:
    case FieldDescriptor::CPPTYPE_MESSAGE:
      GOOGLE_LOG(ERROR) << "Not supported.";
      break;
    case FieldDescriptor::CPPTYPE_STRING:
      reflection->SetString(message, field_desc, key.GetStringValue());
      return;
    case FieldDescriptor::CPPTYPE_INT64:
      reflection->SetInt64(message, field_desc, key.GetInt64Value());
      return;
    case FieldDescriptor::CPPTYPE_INT32:
      reflection->SetInt32(message, field_desc, key.GetInt32Value());
      return;
    case FieldDescriptor::CPPTYPE_UINT64:
      reflection->SetUInt64(message, field_desc, key.GetUInt64Value());
      return;
    case FieldDescriptor::CPPTYPE_UINT32:
      reflection->SetUInt32(message, field_desc, key.GetUInt32Value());
      return;
    case FieldDescriptor::CPPTYPE_BOOL:
      reflection->SetBool(message, field_desc, key.GetBoolValue());
      return;
  }
}

void MapFieldPrinterHelper::CopyValue(const MapValueRef& value,
                                      Message* message,
                                      const FieldDescriptor* field_desc) {
  const Reflection* reflection = message->GetReflection();
  switch (field_desc->cpp_type()) {
    case FieldDescriptor::CPPTYPE_DOUBLE:
      reflection->SetDouble(message, field_desc, value.GetDoubleValue());
      return;
    case FieldDescriptor::CPPTYPE_FLOAT:
      reflection->SetFloat(message, field_desc, value.GetFloatValue());
      return;
    case FieldDescriptor::CPPTYPE_ENUM:
      reflection->SetEnumValue(message, field_desc, value.GetEnumValue());
      return;
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      Message* sub_message = value.GetMessageValue().New();
      sub_message->CopyFrom(value.GetMessageValue());
      reflection->SetAllocatedMessage(message, sub_message, field_desc);
      return;
    }
    case FieldDescriptor::CPPTYPE_STRING:
      reflection->SetString(message, field_desc, value.GetStringValue());
      return;
    case FieldDescriptor::CPPTYPE_INT64:
      reflection->SetInt64(message, field_desc, value.GetInt64Value());
      return;
    case FieldDescriptor::CPPTYPE_INT32:
      reflection->SetInt32(message, field_desc, value.GetInt32Value());
      return;
    case FieldDescriptor::CPPTYPE_UINT64:
      reflection->SetUInt64(message, field_desc, value.GetUInt64Value());
      return;
    case FieldDescriptor::CPPTYPE_UINT32:
      reflection->SetUInt32(message, field_desc, value.GetUInt32Value());
      return;
    case FieldDescriptor::CPPTYPE_BOOL:
      reflection->SetBool(message, field_desc, value.GetBoolValue());
      return;
  }
}
}  // namespace internal

void TextFormat::Printer::PrintField(const Message& message,
                                     const Reflection* reflection,
                                     const FieldDescriptor* field,
                                     TextGenerator* generator) const {
  if (use_short_repeated_primitives_ && field->is_repeated() &&
      field->cpp_type() != FieldDescriptor::CPPTYPE_STRING &&
      field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) {
    PrintShortRepeatedField(message, reflection, field, generator);
    return;
  }

  int count = 0;

  if (field->is_repeated()) {
    count = reflection->FieldSize(message, field);
  } else if (reflection->HasField(message, field) ||
             field->containing_type()->options().map_entry()) {
    count = 1;
  }

  DynamicMessageFactory factory;
  std::vector<const Message*> sorted_map_field;
  bool need_release = false;
  bool is_map = field->is_map();
  if (is_map) {
    need_release = internal::MapFieldPrinterHelper::SortMap(
        message, reflection, field, &factory, &sorted_map_field);
  }

  for (int j = 0; j < count; ++j) {
    const int field_index = field->is_repeated() ? j : -1;

    PrintFieldName(message, field_index, count, reflection, field, generator);

    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      const FastFieldValuePrinter* printer = FindWithDefault(
          custom_printers_, field, default_field_value_printer_.get());
      const Message& sub_message =
          field->is_repeated()
              ? (is_map ? *sorted_map_field[j]
                        : reflection->GetRepeatedMessage(message, field, j))
              : reflection->GetMessage(message, field);
      printer->PrintMessageStart(sub_message, field_index, count,
                                 single_line_mode_, generator);
      generator->Indent();
      Print(sub_message, generator);
      generator->Outdent();
      printer->PrintMessageEnd(sub_message, field_index, count,
                               single_line_mode_, generator);
    } else {
      generator->PrintLiteral(": ");
      // Write the field value.
      PrintFieldValue(message, reflection, field, field_index, generator);
      if (single_line_mode_) {
        generator->PrintLiteral(" ");
      } else {
        generator->PrintLiteral("\n");
      }
    }
  }

  if (need_release) {
    for (int j = 0; j < sorted_map_field.size(); ++j) {
      delete sorted_map_field[j];
    }
  }
}

void TextFormat::Printer::PrintShortRepeatedField(
    const Message& message, const Reflection* reflection,
    const FieldDescriptor* field, TextGenerator* generator) const {
  // Print primitive repeated field in short form.
  int size = reflection->FieldSize(message, field);
  PrintFieldName(message, /*field_index=*/-1, /*field_count=*/size, reflection,
                 field, generator);
  generator->PrintLiteral(": [");
  for (int i = 0; i < size; i++) {
    if (i > 0) generator->PrintLiteral(", ");
    PrintFieldValue(message, reflection, field, i, generator);
  }
  if (single_line_mode_) {
    generator->PrintLiteral("] ");
  } else {
    generator->PrintLiteral("]\n");
  }
}

void TextFormat::Printer::PrintFieldName(const Message& message,
                                         int field_index, int field_count,
                                         const Reflection* reflection,
                                         const FieldDescriptor* field,
                                         TextGenerator* generator) const {
  // if use_field_number_ is true, prints field number instead
  // of field name.
  if (use_field_number_) {
    generator->PrintString(StrCat(field->number()));
    return;
  }

  const FastFieldValuePrinter* printer = FindWithDefault(
      custom_printers_, field, default_field_value_printer_.get());
  printer->PrintFieldName(message, field_index, field_count, reflection, field,
                          generator);
}

void TextFormat::Printer::PrintFieldValue(const Message& message,
                                          const Reflection* reflection,
                                          const FieldDescriptor* field,
                                          int index,
                                          TextGenerator* generator) const {
  GOOGLE_DCHECK(field->is_repeated() || (index == -1))
      << "Index must be -1 for non-repeated fields";

  const FastFieldValuePrinter* printer = FindWithDefault(
      custom_printers_, field, default_field_value_printer_.get());

  switch (field->cpp_type()) {
#define OUTPUT_FIELD(CPPTYPE, METHOD)                                \
  case FieldDescriptor::CPPTYPE_##CPPTYPE:                           \
    printer->Print##METHOD(                                          \
        field->is_repeated()                                         \
            ? reflection->GetRepeated##METHOD(message, field, index) \
            : reflection->Get##METHOD(message, field),               \
        generator);                                                  \
    break

    OUTPUT_FIELD(INT32, Int32);
    OUTPUT_FIELD(INT64, Int64);
    OUTPUT_FIELD(UINT32, UInt32);
    OUTPUT_FIELD(UINT64, UInt64);
    OUTPUT_FIELD(FLOAT, Float);
    OUTPUT_FIELD(DOUBLE, Double);
    OUTPUT_FIELD(BOOL, Bool);
#undef OUTPUT_FIELD

    case FieldDescriptor::CPPTYPE_STRING: {
      std::string scratch;
      const std::string& value =
          field->is_repeated()
              ? reflection->GetRepeatedStringReference(message, field, index,
                                                       &scratch)
              : reflection->GetStringReference(message, field, &scratch);
      const std::string* value_to_print = &value;
      std::string truncated_value;
      if (truncate_string_field_longer_than_ > 0 &&
          truncate_string_field_longer_than_ < value.size()) {
        truncated_value = value.substr(0, truncate_string_field_longer_than_) +
                          "...<truncated>...";
        value_to_print = &truncated_value;
      }
      if (field->type() == FieldDescriptor::TYPE_STRING) {
        printer->PrintString(*value_to_print, generator);
      } else {
        GOOGLE_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
        printer->PrintBytes(*value_to_print, generator);
      }
      break;
    }

    case FieldDescriptor::CPPTYPE_ENUM: {
      int enum_value =
          field->is_repeated()
              ? reflection->GetRepeatedEnumValue(message, field, index)
              : reflection->GetEnumValue(message, field);
      const EnumValueDescriptor* enum_desc =
          field->enum_type()->FindValueByNumber(enum_value);
      if (enum_desc != nullptr) {
        printer->PrintEnum(enum_value, enum_desc->name(), generator);
      } else {
        // Ordinarily, enum_desc should not be null, because proto2 has the
        // invariant that set enum field values must be in-range, but with the
        // new integer-based API for enums (or the RepeatedField<int> loophole),
        // it is possible for the user to force an unknown integer value.  So we
        // simply use the integer value itself as the enum value name in this
        // case.
        printer->PrintEnum(enum_value, StringPrintf("%d", enum_value),
                           generator);
      }
      break;
    }

    case FieldDescriptor::CPPTYPE_MESSAGE:
      Print(field->is_repeated()
                ? reflection->GetRepeatedMessage(message, field, index)
                : reflection->GetMessage(message, field),
            generator);
      break;
  }
}

/* static */ bool TextFormat::Print(const Message& message,
                                    io::ZeroCopyOutputStream* output) {
  return Printer().Print(message, output);
}

/* static */ bool TextFormat::PrintUnknownFields(
    const UnknownFieldSet& unknown_fields, io::ZeroCopyOutputStream* output) {
  return Printer().PrintUnknownFields(unknown_fields, output);
}

/* static */ bool TextFormat::PrintToString(const Message& message,
                                            std::string* output) {
  return Printer().PrintToString(message, output);
}

/* static */ bool TextFormat::PrintUnknownFieldsToString(
    const UnknownFieldSet& unknown_fields, std::string* output) {
  return Printer().PrintUnknownFieldsToString(unknown_fields, output);
}

/* static */ void TextFormat::PrintFieldValueToString(
    const Message& message, const FieldDescriptor* field, int index,
    std::string* output) {
  return Printer().PrintFieldValueToString(message, field, index, output);
}

/* static */ bool TextFormat::ParseFieldValueFromString(
    const std::string& input, const FieldDescriptor* field, Message* message) {
  return Parser().ParseFieldValueFromString(input, field, message);
}

void TextFormat::Printer::PrintUnknownFields(
    const UnknownFieldSet& unknown_fields, TextGenerator* generator) const {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const UnknownField& field = unknown_fields.field(i);
    std::string field_number = StrCat(field.number());

    switch (field.type()) {
      case UnknownField::TYPE_VARINT:
        generator->PrintString(field_number);
        generator->PrintLiteral(": ");
        generator->PrintString(StrCat(field.varint()));
        if (single_line_mode_) {
          generator->PrintLiteral(" ");
        } else {
          generator->PrintLiteral("\n");
        }
        break;
      case UnknownField::TYPE_FIXED32: {
        generator->PrintString(field_number);
        generator->PrintLiteral(": 0x");
        generator->PrintString(
            StrCat(strings::Hex(field.fixed32(), strings::ZERO_PAD_8)));
        if (single_line_mode_) {
          generator->PrintLiteral(" ");
        } else {
          generator->PrintLiteral("\n");
        }
        break;
      }
      case UnknownField::TYPE_FIXED64: {
        generator->PrintString(field_number);
        generator->PrintLiteral(": 0x");
        generator->PrintString(
            StrCat(strings::Hex(field.fixed64(), strings::ZERO_PAD_16)));
        if (single_line_mode_) {
          generator->PrintLiteral(" ");
        } else {
          generator->PrintLiteral("\n");
        }
        break;
      }
      case UnknownField::TYPE_LENGTH_DELIMITED: {
        generator->PrintString(field_number);
        const std::string& value = field.length_delimited();
        UnknownFieldSet embedded_unknown_fields;
        if (!value.empty() && embedded_unknown_fields.ParseFromString(value)) {
          // This field is parseable as a Message.
          // So it is probably an embedded message.
          if (single_line_mode_) {
            generator->PrintLiteral(" { ");
          } else {
            generator->PrintLiteral(" {\n");
            generator->Indent();
          }
          PrintUnknownFields(embedded_unknown_fields, generator);
          if (single_line_mode_) {
            generator->PrintLiteral("} ");
          } else {
            generator->Outdent();
            generator->PrintLiteral("}\n");
          }
        } else {
          // This field is not parseable as a Message.
          // So it is probably just a plain string.
          generator->PrintLiteral(": \"");
          generator->PrintString(CEscape(value));
          if (single_line_mode_) {
            generator->PrintLiteral("\" ");
          } else {
            generator->PrintLiteral("\"\n");
          }
        }
        break;
      }
      case UnknownField::TYPE_GROUP:
        generator->PrintString(field_number);
        if (single_line_mode_) {
          generator->PrintLiteral(" { ");
        } else {
          generator->PrintLiteral(" {\n");
          generator->Indent();
        }
        PrintUnknownFields(field.group(), generator);
        if (single_line_mode_) {
          generator->PrintLiteral("} ");
        } else {
          generator->Outdent();
          generator->PrintLiteral("}\n");
        }
        break;
    }
  }
}

}  // namespace protobuf
}  // namespace google
