/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>

#include <map>
#include <set>
#include <stack>
#include <string>

#include "src/perfetto_cmd/pbtxt_to_pb.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/message.h"
#include "perfetto/protozero/message_handle.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/perfetto_cmd/perfetto_config.descriptor.h"

#include "protos/perfetto/common/descriptor.gen.h"

namespace perfetto {
constexpr char kConfigProtoName[] = ".perfetto.protos.TraceConfig";

using protos::gen::DescriptorProto;
using protos::gen::EnumDescriptorProto;
using protos::gen::EnumValueDescriptorProto;
using protos::gen::FieldDescriptorProto;
using protos::gen::FileDescriptorSet;

namespace {

constexpr bool IsIdentifierStart(char c) {
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || c == '_';
}

constexpr bool IsIdentifierBody(char c) {
  return IsIdentifierStart(c) || isdigit(c);
}

const char* FieldToTypeName(const FieldDescriptorProto* field) {
  switch (field->type()) {
    case FieldDescriptorProto::TYPE_UINT64:
      return "uint64";
    case FieldDescriptorProto::TYPE_UINT32:
      return "uint32";
    case FieldDescriptorProto::TYPE_INT64:
      return "int64";
    case FieldDescriptorProto::TYPE_SINT64:
      return "sint64";
    case FieldDescriptorProto::TYPE_INT32:
      return "int32";
    case FieldDescriptorProto::TYPE_SINT32:
      return "sint32";
    case FieldDescriptorProto::TYPE_FIXED64:
      return "fixed64";
    case FieldDescriptorProto::TYPE_SFIXED64:
      return "sfixed64";
    case FieldDescriptorProto::TYPE_FIXED32:
      return "fixed32";
    case FieldDescriptorProto::TYPE_SFIXED32:
      return "sfixed32";
    case FieldDescriptorProto::TYPE_DOUBLE:
      return "double";
    case FieldDescriptorProto::TYPE_FLOAT:
      return "float";
    case FieldDescriptorProto::TYPE_BOOL:
      return "bool";
    case FieldDescriptorProto::TYPE_STRING:
      return "string";
    case FieldDescriptorProto::TYPE_BYTES:
      return "bytes";
    case FieldDescriptorProto::TYPE_GROUP:
      return "group";
    case FieldDescriptorProto::TYPE_MESSAGE:
      return "message";
    case FieldDescriptorProto::TYPE_ENUM:
      return "enum";
  }
  // For gcc
  PERFETTO_FATAL("Non complete switch");
}

std::string Format(const char* fmt, std::map<std::string, std::string> args) {
  std::string result(fmt);
  for (const auto& key_value : args) {
    size_t start = result.find(key_value.first);
    PERFETTO_CHECK(start != std::string::npos);
    result.replace(start, key_value.first.size(), key_value.second);
    PERFETTO_CHECK(result.find(key_value.first) == std::string::npos);
  }
  return result;
}

enum ParseState {
  kWaitingForKey,
  kReadingKey,
  kWaitingForValue,
  kReadingStringValue,
  kReadingStringEscape,
  kReadingNumericValue,
  kReadingIdentifierValue,
};

struct Token {
  size_t offset;
  size_t column;
  size_t row;
  base::StringView txt;

  size_t size() const { return txt.size(); }
  std::string ToStdString() const { return txt.ToStdString(); }
};

struct ParserDelegateContext {
  const DescriptorProto* descriptor;
  protozero::Message* message;
  std::set<std::string> seen_fields;
};

class ParserDelegate {
 public:
  ParserDelegate(
      const DescriptorProto* descriptor,
      protozero::Message* message,
      ErrorReporter* reporter,
      std::map<std::string, const DescriptorProto*> name_to_descriptor,
      std::map<std::string, const EnumDescriptorProto*> name_to_enum)
      : reporter_(reporter),
        name_to_descriptor_(std::move(name_to_descriptor)),
        name_to_enum_(std::move(name_to_enum)) {
    ctx_.push(ParserDelegateContext{descriptor, message, {}});
  }

  void NumericField(Token key, Token value) {
    const FieldDescriptorProto* field = FindFieldByName(
        key, value,
        {
            FieldDescriptorProto::TYPE_UINT64,
            FieldDescriptorProto::TYPE_UINT32, FieldDescriptorProto::TYPE_INT64,
            FieldDescriptorProto::TYPE_SINT64, FieldDescriptorProto::TYPE_INT32,
            FieldDescriptorProto::TYPE_SINT32,
            FieldDescriptorProto::TYPE_FIXED64,
            FieldDescriptorProto::TYPE_SFIXED64,
            FieldDescriptorProto::TYPE_FIXED32,
            FieldDescriptorProto::TYPE_SFIXED32,
            FieldDescriptorProto::TYPE_DOUBLE, FieldDescriptorProto::TYPE_FLOAT,
        });
    if (!field)
      return;
    const auto& field_type = field->type();
    switch (field_type) {
      case FieldDescriptorProto::TYPE_UINT64:
        return VarIntField<uint64_t>(field, value);
      case FieldDescriptorProto::TYPE_UINT32:
        return VarIntField<uint32_t>(field, value);
      case FieldDescriptorProto::TYPE_INT64:
      case FieldDescriptorProto::TYPE_SINT64:
        return VarIntField<int64_t>(field, value);
      case FieldDescriptorProto::TYPE_INT32:
      case FieldDescriptorProto::TYPE_SINT32:
        return VarIntField<int32_t>(field, value);

      case FieldDescriptorProto::TYPE_FIXED64:
      case FieldDescriptorProto::TYPE_SFIXED64:
        return FixedField<int64_t>(field, value);

      case FieldDescriptorProto::TYPE_FIXED32:
      case FieldDescriptorProto::TYPE_SFIXED32:
        return FixedField<int32_t>(field, value);

      case FieldDescriptorProto::TYPE_DOUBLE:
        return FixedFloatField<double>(field, value);
      case FieldDescriptorProto::TYPE_FLOAT:
        return FixedFloatField<float>(field, value);

      case FieldDescriptorProto::TYPE_BOOL:
      case FieldDescriptorProto::TYPE_STRING:
      case FieldDescriptorProto::TYPE_BYTES:
      case FieldDescriptorProto::TYPE_GROUP:
      case FieldDescriptorProto::TYPE_MESSAGE:
      case FieldDescriptorProto::TYPE_ENUM:
        PERFETTO_FATAL("Invalid type");
    }
  }

  void StringField(Token key, Token value) {
    const FieldDescriptorProto* field = FindFieldByName(
        key, value,
        {
            FieldDescriptorProto::TYPE_STRING, FieldDescriptorProto::TYPE_BYTES,
        });
    if (!field)
      return;
    uint32_t field_id = static_cast<uint32_t>(field->number());
    const auto& field_type = field->type();
    PERFETTO_CHECK(field_type == FieldDescriptorProto::TYPE_STRING ||
                   field_type == FieldDescriptorProto::TYPE_BYTES);

    std::unique_ptr<char, base::FreeDeleter> s(
        static_cast<char*>(malloc(value.size())));
    size_t j = 0;
    for (size_t i = 0; i < value.size(); i++) {
      char c = value.txt.data()[i];
      if (c == '\\') {
        if (i + 1 >= value.size()) {
          // This should be caught by the lexer.
          PERFETTO_FATAL("Escape at end of string.");
          return;
        }
        char next = value.txt.data()[++i];
        switch (next) {
          case '\\':
          case '\'':
          case '"':
          case '?':
            s.get()[j++] = next;
            break;
          case 'a':
            s.get()[j++] = '\a';
            break;
          case 'b':
            s.get()[j++] = '\b';
            break;
          case 'f':
            s.get()[j++] = '\f';
            break;
          case 'n':
            s.get()[j++] = '\n';
            break;
          case 'r':
            s.get()[j++] = '\r';
            break;
          case 't':
            s.get()[j++] = '\t';
            break;
          case 'v':
            s.get()[j++] = '\v';
            break;
          default:
            AddError(value,
                     "Unknown string escape in $k in "
                     "proto $n: '$v'",
                     std::map<std::string, std::string>{
                         {"$k", key.ToStdString()},
                         {"$n", descriptor_name()},
                         {"$v", value.ToStdString()},
                     });
            return;
        }
      } else {
        s.get()[j++] = c;
      }
    }
    msg()->AppendBytes(field_id, s.get(), j);
  }

  void IdentifierField(Token key, Token value) {
    const FieldDescriptorProto* field = FindFieldByName(
        key, value,
        {
            FieldDescriptorProto::TYPE_BOOL, FieldDescriptorProto::TYPE_ENUM,
        });
    if (!field)
      return;
    uint32_t field_id = static_cast<uint32_t>(field->number());
    const auto& field_type = field->type();
    if (field_type == FieldDescriptorProto::TYPE_BOOL) {
      if (value.txt != "true" && value.txt != "false") {
        AddError(value,
                 "Expected 'true' or 'false' for boolean field $k in "
                 "proto $n instead saw '$v'",
                 std::map<std::string, std::string>{
                     {"$k", key.ToStdString()},
                     {"$n", descriptor_name()},
                     {"$v", value.ToStdString()},
                 });
        return;
      }
      msg()->AppendTinyVarInt(field_id, value.txt == "true" ? 1 : 0);
    } else if (field_type == FieldDescriptorProto::TYPE_ENUM) {
      const std::string& type_name = field->type_name();
      const EnumDescriptorProto* enum_descriptor = name_to_enum_[type_name];
      PERFETTO_CHECK(enum_descriptor);
      bool found_value = false;
      int32_t enum_value_number = 0;
      for (const EnumValueDescriptorProto& enum_value :
           enum_descriptor->value()) {
        if (value.ToStdString() != enum_value.name())
          continue;
        found_value = true;
        enum_value_number = enum_value.number();
        break;
      }
      if (!found_value) {
        AddError(value,
                 "Unexpected value '$v' for enum field $k in "
                 "proto $n",
                 std::map<std::string, std::string>{
                     {"$v", value.ToStdString()},
                     {"$k", key.ToStdString()},
                     {"$n", descriptor_name()},
                 });
        return;
      }
      msg()->AppendVarInt<int32_t>(field_id, enum_value_number);
    }
  }

  void BeginNestedMessage(Token key, Token value) {
    const FieldDescriptorProto* field =
        FindFieldByName(key, value,
                        {
                            FieldDescriptorProto::TYPE_MESSAGE,
                        });
    if (!field)
      return;
    uint32_t field_id = static_cast<uint32_t>(field->number());
    const std::string& type_name = field->type_name();
    const DescriptorProto* nested_descriptor = name_to_descriptor_[type_name];
    PERFETTO_CHECK(nested_descriptor);
    auto* nested_msg = msg()->BeginNestedMessage<protozero::Message>(field_id);
    ctx_.push(ParserDelegateContext{nested_descriptor, nested_msg, {}});
  }

  void EndNestedMessage() {
    msg()->Finalize();
    ctx_.pop();
  }

  void Eof() {}

  void AddError(size_t row,
                size_t column,
                const char* fmt,
                const std::map<std::string, std::string>& args) {
    reporter_->AddError(row, column, 0, Format(fmt, args));
  }

  void AddError(Token token,
                const char* fmt,
                const std::map<std::string, std::string>& args) {
    reporter_->AddError(token.row, token.column, token.size(),
                        Format(fmt, args));
  }

 private:
  template <typename T>
  void VarIntField(const FieldDescriptorProto* field, Token t) {
    uint32_t field_id = static_cast<uint32_t>(field->number());
    uint64_t n = 0;
    PERFETTO_CHECK(ParseInteger(t.txt, &n));
    if (field->type() == FieldDescriptorProto::TYPE_SINT64 ||
        field->type() == FieldDescriptorProto::TYPE_SINT32) {
      msg()->AppendSignedVarInt<T>(field_id, static_cast<T>(n));
    } else {
      msg()->AppendVarInt<T>(field_id, static_cast<T>(n));
    }
  }

  template <typename T>
  void FixedField(const FieldDescriptorProto* field, Token t) {
    uint32_t field_id = static_cast<uint32_t>(field->number());
    uint64_t n = 0;
    PERFETTO_CHECK(ParseInteger(t.txt, &n));
    msg()->AppendFixed<T>(field_id, static_cast<T>(n));
  }

  template <typename T>
  void FixedFloatField(const FieldDescriptorProto* field, Token t) {
    uint32_t field_id = static_cast<uint32_t>(field->number());
    base::Optional<double> opt_n = base::StringToDouble(t.ToStdString());
    msg()->AppendFixed<T>(field_id, static_cast<T>(opt_n.value_or(0l)));
  }

  template <typename T>
  bool ParseInteger(base::StringView s, T* number_ptr) {
    uint64_t n = 0;
    PERFETTO_CHECK(sscanf(s.ToStdString().c_str(), "%" PRIu64, &n) == 1);
    PERFETTO_CHECK(n <= std::numeric_limits<T>::max());
    *number_ptr = static_cast<T>(n);
    return true;
  }

  const FieldDescriptorProto* FindFieldByName(
      Token key,
      Token value,
      std::set<FieldDescriptorProto::Type> valid_field_types) {
    const std::string field_name = key.ToStdString();
    const FieldDescriptorProto* field_descriptor = nullptr;
    for (const auto& f : descriptor()->field()) {
      if (f.name() == field_name) {
        field_descriptor = &f;
        break;
      }
    }

    if (!field_descriptor) {
      AddError(key, "No field named \"$n\" in proto $p",
               {
                   {"$n", field_name}, {"$p", descriptor_name()},
               });
      return nullptr;
    }

    bool is_repeated =
        field_descriptor->label() == FieldDescriptorProto::LABEL_REPEATED;
    auto it_and_inserted = ctx_.top().seen_fields.emplace(field_name);
    if (!it_and_inserted.second && !is_repeated) {
      AddError(key, "Saw non-repeating field '$f' more than once",
               {
                   {"$f", field_name},
               });
    }

    if (!valid_field_types.count(field_descriptor->type())) {
      AddError(value,
               "Expected value of type $t for field $k in proto $n "
               "instead saw '$v'",
               {
                   {"$t", FieldToTypeName(field_descriptor)},
                   {"$k", field_name},
                   {"$n", descriptor_name()},
                   {"$v", value.ToStdString()},
               });
      return nullptr;
    }

    return field_descriptor;
  }

  const DescriptorProto* descriptor() {
    PERFETTO_CHECK(!ctx_.empty());
    return ctx_.top().descriptor;
  }

  const std::string& descriptor_name() { return descriptor()->name(); }

  protozero::Message* msg() {
    PERFETTO_CHECK(!ctx_.empty());
    return ctx_.top().message;
  }

  std::stack<ParserDelegateContext> ctx_;
  ErrorReporter* reporter_;
  std::map<std::string, const DescriptorProto*> name_to_descriptor_;
  std::map<std::string, const EnumDescriptorProto*> name_to_enum_;
};

void Parse(const std::string& input, ParserDelegate* delegate) {
  ParseState state = kWaitingForKey;
  size_t column = 0;
  size_t row = 1;
  size_t depth = 0;
  bool saw_colon_for_this_key = false;
  bool saw_semicolon_for_this_value = true;
  bool comment_till_eol = false;
  Token key{};
  Token value{};

  for (size_t i = 0; i < input.size(); i++, column++) {
    bool last_character = i + 1 == input.size();
    char c = input.at(i);
    if (c == '\n') {
      column = 0;
      row++;
      if (comment_till_eol) {
        comment_till_eol = false;
        continue;
      }
    }
    if (comment_till_eol)
      continue;

    switch (state) {
      case kWaitingForKey:
        if (isspace(c))
          continue;
        if (c == '#') {
          comment_till_eol = true;
          continue;
        }
        if (c == '}') {
          if (depth == 0) {
            delegate->AddError(row, column, "Unmatched closing brace", {});
            return;
          }
          saw_semicolon_for_this_value = false;
          depth--;
          delegate->EndNestedMessage();
          continue;
        }
        if (!saw_semicolon_for_this_value && c == ';') {
          saw_semicolon_for_this_value = true;
          continue;
        }
        if (IsIdentifierStart(c)) {
          saw_colon_for_this_key = false;
          state = kReadingKey;
          key.offset = i;
          key.row = row;
          key.column = column;
          continue;
        }
        break;

      case kReadingKey:
        if (IsIdentifierBody(c))
          continue;
        key.txt = base::StringView(input.data() + key.offset, i - key.offset);
        state = kWaitingForValue;
        if (c == '#')
          comment_till_eol = true;
        continue;

      case kWaitingForValue:
        if (isspace(c))
          continue;
        if (c == '#') {
          comment_till_eol = true;
          continue;
        }
        value.offset = i;
        value.row = row;
        value.column = column;

        if (c == ':' && !saw_colon_for_this_key) {
          saw_colon_for_this_key = true;
          continue;
        }
        if (c == '"') {
          state = kReadingStringValue;
          continue;
        }
        if (c == '-' || isdigit(c)) {
          state = kReadingNumericValue;
          continue;
        }
        if (IsIdentifierStart(c)) {
          state = kReadingIdentifierValue;
          continue;
        }
        if (c == '{') {
          state = kWaitingForKey;
          depth++;
          value.txt = base::StringView(input.data() + value.offset, 1);
          delegate->BeginNestedMessage(key, value);
          continue;
        }
        break;

      case kReadingNumericValue:
        if (isspace(c) || c == ';' || last_character) {
          bool keep_last = last_character && !(isspace(c) || c == ';');
          size_t size = i - value.offset + (keep_last ? 1 : 0);
          value.txt = base::StringView(input.data() + value.offset, size);
          saw_semicolon_for_this_value = c == ';';
          state = kWaitingForKey;
          delegate->NumericField(key, value);
          continue;
        }
        if (isdigit(c))
          continue;
        break;

      case kReadingStringValue:
        if (c == '\\') {
          state = kReadingStringEscape;
        } else if (c == '"') {
          size_t size = i - value.offset - 1;
          value.column++;
          value.txt = base::StringView(input.data() + value.offset + 1, size);
          saw_semicolon_for_this_value = false;
          state = kWaitingForKey;
          delegate->StringField(key, value);
        }
        continue;

      case kReadingStringEscape:
        state = kReadingStringValue;
        continue;

      case kReadingIdentifierValue:
        if (isspace(c) || c == ';' || c == '#' || last_character) {
          bool keep_last =
              last_character && !(isspace(c) || c == ';' || c == '#');
          size_t size = i - value.offset + (keep_last ? 1 : 0);
          value.txt = base::StringView(input.data() + value.offset, size);
          comment_till_eol = c == '#';
          saw_semicolon_for_this_value = c == ';';
          state = kWaitingForKey;
          delegate->IdentifierField(key, value);
          continue;
        }
        if (IsIdentifierBody(c)) {
          continue;
        }
        break;
    }
    PERFETTO_FATAL("Unexpected char %c", c);
  }  // for
  if (depth > 0)
    delegate->AddError(row, column, "Nested message not closed", {});
  if (state != kWaitingForKey)
    delegate->AddError(row, column, "Unexpected end of input", {});
  delegate->Eof();
}

void AddNestedDescriptors(
    const std::string& prefix,
    const DescriptorProto* descriptor,
    std::map<std::string, const DescriptorProto*>* name_to_descriptor,
    std::map<std::string, const EnumDescriptorProto*>* name_to_enum) {
  for (const EnumDescriptorProto& enum_descriptor : descriptor->enum_type()) {
    const std::string name = prefix + "." + enum_descriptor.name();
    (*name_to_enum)[name] = &enum_descriptor;
  }
  for (const DescriptorProto& nested_descriptor : descriptor->nested_type()) {
    const std::string name = prefix + "." + nested_descriptor.name();
    (*name_to_descriptor)[name] = &nested_descriptor;
    AddNestedDescriptors(name, &nested_descriptor, name_to_descriptor,
                         name_to_enum);
  }
}

}  // namespace

ErrorReporter::ErrorReporter() = default;
ErrorReporter::~ErrorReporter() = default;

std::vector<uint8_t> PbtxtToPb(const std::string& input,
                               ErrorReporter* reporter) {
  std::map<std::string, const DescriptorProto*> name_to_descriptor;
  std::map<std::string, const EnumDescriptorProto*> name_to_enum;
  FileDescriptorSet file_descriptor_set;

  {
    file_descriptor_set.ParseFromArray(
        kPerfettoConfigDescriptor.data(),
        static_cast<int>(kPerfettoConfigDescriptor.size()));
    for (const auto& file_descriptor : file_descriptor_set.file()) {
      for (const auto& enum_descriptor : file_descriptor.enum_type()) {
        const std::string name =
            "." + file_descriptor.package() + "." + enum_descriptor.name();
        name_to_enum[name] = &enum_descriptor;
      }
      for (const auto& descriptor : file_descriptor.message_type()) {
        const std::string name =
            "." + file_descriptor.package() + "." + descriptor.name();
        name_to_descriptor[name] = &descriptor;
        AddNestedDescriptors(name, &descriptor, &name_to_descriptor,
                             &name_to_enum);
      }
    }
  }

  const DescriptorProto* descriptor = name_to_descriptor[kConfigProtoName];
  PERFETTO_CHECK(descriptor);

  protozero::HeapBuffered<protozero::Message> message;
  ParserDelegate delegate(descriptor, message.get(), reporter,
                          std::move(name_to_descriptor),
                          std::move(name_to_enum));
  Parse(input, &delegate);
  return message.SerializeAsArray();
}

}  // namespace perfetto
