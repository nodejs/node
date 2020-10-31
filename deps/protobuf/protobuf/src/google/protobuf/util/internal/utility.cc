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

#include <google/protobuf/util/internal/utility.h>

#include <algorithm>

#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/wrappers.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/internal/constants.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/mathlimits.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

bool GetBoolOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, bool default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetBoolFromAny(opt->value());
}

int64 GetInt64OptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, int64 default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetInt64FromAny(opt->value());
}

double GetDoubleOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, double default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetDoubleFromAny(opt->value());
}

std::string GetStringOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, const std::string& default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetStringFromAny(opt->value());
}

template <typename T>
void ParseFromAny(const std::string& data, T* result) {
  result->ParseFromString(data);
}

// Returns a boolean value contained in Any type.
// TODO(skarvaje): Add type checking & error messages here.
bool GetBoolFromAny(const google::protobuf::Any& any) {
  google::protobuf::BoolValue b;
  ParseFromAny(any.value(), &b);
  return b.value();
}

int64 GetInt64FromAny(const google::protobuf::Any& any) {
  google::protobuf::Int64Value i;
  ParseFromAny(any.value(), &i);
  return i.value();
}

double GetDoubleFromAny(const google::protobuf::Any& any) {
  google::protobuf::DoubleValue i;
  ParseFromAny(any.value(), &i);
  return i.value();
}

std::string GetStringFromAny(const google::protobuf::Any& any) {
  google::protobuf::StringValue s;
  ParseFromAny(any.value(), &s);
  return s.value();
}

const StringPiece GetTypeWithoutUrl(StringPiece type_url) {
  if (type_url.size() > kTypeUrlSize && type_url[kTypeUrlSize] == '/') {
    return type_url.substr(kTypeUrlSize + 1);
  } else {
    size_t idx = type_url.rfind('/');
    if (idx != type_url.npos) {
      type_url.remove_prefix(idx + 1);
    }
    return type_url;
  }
}

const std::string GetFullTypeWithUrl(StringPiece simple_type) {
  return StrCat(kTypeServiceBaseUrl, "/", simple_type);
}

const google::protobuf::Option* FindOptionOrNull(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name) {
  for (int i = 0; i < options.size(); ++i) {
    const google::protobuf::Option& opt = options.Get(i);
    if (opt.name() == option_name) {
      return &opt;
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindFieldInTypeOrNull(
    const google::protobuf::Type* type, StringPiece field_name) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.name() == field_name) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindJsonFieldInTypeOrNull(
    const google::protobuf::Type* type, StringPiece json_name) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.json_name() == json_name) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindFieldInTypeByNumberOrNull(
    const google::protobuf::Type* type, int32 number) {
  if (type != nullptr) {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      if (field.number() == number) {
        return &field;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNameOrNull(
    const google::protobuf::Enum* enum_type, StringPiece enum_name) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      if (enum_value.name() == enum_name) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNumberOrNull(
    const google::protobuf::Enum* enum_type, int32 value) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      if (enum_value.number() == value) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

const google::protobuf::EnumValue* FindEnumValueByNameWithoutUnderscoreOrNull(
    const google::protobuf::Enum* enum_type, StringPiece enum_name) {
  if (enum_type != nullptr) {
    for (int i = 0; i < enum_type->enumvalue_size(); ++i) {
      const google::protobuf::EnumValue& enum_value = enum_type->enumvalue(i);
      std::string enum_name_without_underscore = enum_value.name();

      // Remove underscore from the name.
      enum_name_without_underscore.erase(
          std::remove(enum_name_without_underscore.begin(),
                      enum_name_without_underscore.end(), '_'),
          enum_name_without_underscore.end());
      // Make the name uppercase.
      for (std::string::iterator it = enum_name_without_underscore.begin();
           it != enum_name_without_underscore.end(); ++it) {
        *it = ascii_toupper(*it);
      }

      if (enum_name_without_underscore == enum_name) {
        return &enum_value;
      }
    }
  }
  return nullptr;
}

std::string EnumValueNameToLowerCamelCase(const StringPiece input) {
  std::string input_string(input);
  std::transform(input_string.begin(), input_string.end(), input_string.begin(),
                 ::tolower);
  return ToCamelCase(input_string);
}

std::string ToCamelCase(const StringPiece input) {
  bool capitalize_next = false;
  bool was_cap = true;
  bool is_cap = false;
  bool first_word = true;
  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i, was_cap = is_cap) {
    is_cap = ascii_isupper(input[i]);
    if (input[i] == '_') {
      capitalize_next = true;
      if (!result.empty()) first_word = false;
      continue;
    } else if (first_word) {
      // Consider when the current character B is capitalized,
      // first word ends when:
      // 1) following a lowercase:   "...aB..."
      // 2) followed by a lowercase: "...ABc..."
      if (!result.empty() && is_cap &&
          (!was_cap || (i + 1 < input.size() && ascii_islower(input[i + 1])))) {
        first_word = false;
        result.push_back(input[i]);
      } else {
        result.push_back(ascii_tolower(input[i]));
        continue;
      }
    } else if (capitalize_next) {
      capitalize_next = false;
      if (ascii_islower(input[i])) {
        result.push_back(ascii_toupper(input[i]));
        continue;
      } else {
        result.push_back(input[i]);
        continue;
      }
    } else {
      result.push_back(ascii_tolower(input[i]));
    }
  }
  return result;
}

std::string ToSnakeCase(StringPiece input) {
  bool was_not_underscore = false;  // Initialize to false for case 1 (below)
  bool was_not_cap = false;
  std::string result;
  result.reserve(input.size() << 1);

  for (size_t i = 0; i < input.size(); ++i) {
    if (ascii_isupper(input[i])) {
      // Consider when the current character B is capitalized:
      // 1) At beginning of input:   "B..." => "b..."
      //    (e.g. "Biscuit" => "biscuit")
      // 2) Following a lowercase:   "...aB..." => "...a_b..."
      //    (e.g. "gBike" => "g_bike")
      // 3) At the end of input:     "...AB" => "...ab"
      //    (e.g. "GoogleLAB" => "google_lab")
      // 4) Followed by a lowercase: "...ABc..." => "...a_bc..."
      //    (e.g. "GBike" => "g_bike")
      if (was_not_underscore &&               //            case 1 out
          (was_not_cap ||                     // case 2 in, case 3 out
           (i + 1 < input.size() &&           //            case 3 out
            ascii_islower(input[i + 1])))) {  // case 4 in
        // We add an underscore for case 2 and case 4.
        result.push_back('_');
      }
      result.push_back(ascii_tolower(input[i]));
      was_not_underscore = true;
      was_not_cap = false;
    } else {
      result.push_back(input[i]);
      was_not_underscore = input[i] != '_';
      was_not_cap = true;
    }
  }
  return result;
}

std::set<std::string>* well_known_types_ = NULL;
PROTOBUF_NAMESPACE_ID::internal::once_flag well_known_types_init_;
const char* well_known_types_name_array_[] = {
    "google.protobuf.Timestamp",   "google.protobuf.Duration",
    "google.protobuf.DoubleValue", "google.protobuf.FloatValue",
    "google.protobuf.Int64Value",  "google.protobuf.UInt64Value",
    "google.protobuf.Int32Value",  "google.protobuf.UInt32Value",
    "google.protobuf.BoolValue",   "google.protobuf.StringValue",
    "google.protobuf.BytesValue",  "google.protobuf.FieldMask"};

void DeleteWellKnownTypes() { delete well_known_types_; }

void InitWellKnownTypes() {
  well_known_types_ = new std::set<std::string>;
  for (int i = 0; i < GOOGLE_ARRAYSIZE(well_known_types_name_array_); ++i) {
    well_known_types_->insert(well_known_types_name_array_[i]);
  }
  google::protobuf::internal::OnShutdown(&DeleteWellKnownTypes);
}

bool IsWellKnownType(const std::string& type_name) {
  PROTOBUF_NAMESPACE_ID::internal::call_once(well_known_types_init_,
                                             InitWellKnownTypes);
  return ContainsKey(*well_known_types_, type_name);
}

bool IsValidBoolString(const std::string& bool_string) {
  return bool_string == "true" || bool_string == "false" ||
         bool_string == "1" || bool_string == "0";
}

bool IsMap(const google::protobuf::Field& field,
           const google::protobuf::Type& type) {
  return field.cardinality() ==
             google::protobuf::Field_Cardinality_CARDINALITY_REPEATED &&
         (GetBoolOptionOrDefault(type.options(), "map_entry", false) ||
          GetBoolOptionOrDefault(type.options(),
                                 "google.protobuf.MessageOptions.map_entry",
                                 false));
}

bool IsMessageSetWireFormat(const google::protobuf::Type& type) {
  return GetBoolOptionOrDefault(type.options(), "message_set_wire_format",
                                false) ||
         GetBoolOptionOrDefault(
             type.options(),
             "google.protobuf.MessageOptions.message_set_wire_format", false);
}

std::string DoubleAsString(double value) {
  if (MathLimits<double>::IsPosInf(value)) return "Infinity";
  if (MathLimits<double>::IsNegInf(value)) return "-Infinity";
  if (MathLimits<double>::IsNaN(value)) return "NaN";

  return SimpleDtoa(value);
}

std::string FloatAsString(float value) {
  if (MathLimits<float>::IsFinite(value)) return SimpleFtoa(value);
  return DoubleAsString(value);
}

bool SafeStrToFloat(StringPiece str, float* value) {
  double double_value;
  if (!safe_strtod(str, &double_value)) {
    return false;
  }

  if (MathLimits<double>::IsInf(double_value) ||
      MathLimits<double>::IsNaN(double_value))
    return false;

  // Fail if the value is not representable in float.
  if (double_value > std::numeric_limits<float>::max() ||
      double_value < -std::numeric_limits<float>::max()) {
    return false;
  }

  *value = static_cast<float>(double_value);
  return true;
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
