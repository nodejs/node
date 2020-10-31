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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H__

#include <memory>
#include <string>
#include <utility>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/type.pb.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stringpiece.h>

#include <google/protobuf/stubs/status.h>
#include <google/protobuf/stubs/statusor.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// Size of "type.googleapis.com"
static const int64 kTypeUrlSize = 19;

// Finds the tech option identified by option_name. Parses the boolean value and
// returns it.
// When the option with the given name is not found, default_value is returned.
PROTOBUF_EXPORT bool GetBoolOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, bool default_value);

// Returns int64 option value. If the option isn't found, returns the
// default_value.
PROTOBUF_EXPORT int64 GetInt64OptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, int64 default_value);

// Returns double option value. If the option isn't found, returns the
// default_value.
PROTOBUF_EXPORT double GetDoubleOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, double default_value);

// Returns string option value. If the option isn't found, returns the
// default_value.
PROTOBUF_EXPORT std::string GetStringOptionOrDefault(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, const std::string& default_value);

// Returns a boolean value contained in Any type.
// TODO(skarvaje): Make these utilities dealing with Any types more generic,
// add more error checking and move to a more public/sharable location so others
// can use.
PROTOBUF_EXPORT bool GetBoolFromAny(const google::protobuf::Any& any);

// Returns int64 value contained in Any type.
PROTOBUF_EXPORT int64 GetInt64FromAny(const google::protobuf::Any& any);

// Returns double value contained in Any type.
PROTOBUF_EXPORT double GetDoubleFromAny(const google::protobuf::Any& any);

// Returns string value contained in Any type.
PROTOBUF_EXPORT std::string GetStringFromAny(const google::protobuf::Any& any);

// Returns the type string without the url prefix. e.g.: If the passed type is
// 'type.googleapis.com/tech.type.Bool', the returned value is 'tech.type.Bool'.
PROTOBUF_EXPORT const StringPiece GetTypeWithoutUrl(
    StringPiece type_url);

// Returns the simple_type with the base type url (kTypeServiceBaseUrl)
// prefixed.
//
// E.g:
// GetFullTypeWithUrl("google.protobuf.Timestamp") returns the string
// "type.googleapis.com/google.protobuf.Timestamp".
PROTOBUF_EXPORT const std::string GetFullTypeWithUrl(
    StringPiece simple_type);

// Finds and returns option identified by name and option_name within the
// provided map. Returns nullptr if none found.
const google::protobuf::Option* FindOptionOrNull(
    const RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name);

// Finds and returns the field identified by field_name in the passed tech Type
// object. Returns nullptr if none found.
const google::protobuf::Field* FindFieldInTypeOrNull(
    const google::protobuf::Type* type, StringPiece field_name);

// Similar to FindFieldInTypeOrNull, but this looks up fields with given
// json_name.
const google::protobuf::Field* FindJsonFieldInTypeOrNull(
    const google::protobuf::Type* type, StringPiece json_name);

// Similar to FindFieldInTypeOrNull, but this looks up fields by number.
const google::protobuf::Field* FindFieldInTypeByNumberOrNull(
    const google::protobuf::Type* type, int32 number);

// Finds and returns the EnumValue identified by enum_name in the passed tech
// Enum object. Returns nullptr if none found.
const google::protobuf::EnumValue* FindEnumValueByNameOrNull(
    const google::protobuf::Enum* enum_type, StringPiece enum_name);

// Finds and returns the EnumValue identified by value in the passed tech
// Enum object. Returns nullptr if none found.
const google::protobuf::EnumValue* FindEnumValueByNumberOrNull(
    const google::protobuf::Enum* enum_type, int32 value);

// Finds and returns the EnumValue identified by enum_name without underscore in
// the passed tech Enum object. Returns nullptr if none found.
// For Ex. if enum_name is ACTIONANDADVENTURE it can get accepted if
// EnumValue's name is action_and_adventure or ACTION_AND_ADVENTURE.
const google::protobuf::EnumValue* FindEnumValueByNameWithoutUnderscoreOrNull(
    const google::protobuf::Enum* enum_type, StringPiece enum_name);

// Converts input to camel-case and returns it.
PROTOBUF_EXPORT std::string ToCamelCase(const StringPiece input);

// Converts enum name string to camel-case and returns it.
std::string EnumValueNameToLowerCamelCase(const StringPiece input);

// Converts input to snake_case and returns it.
PROTOBUF_EXPORT std::string ToSnakeCase(StringPiece input);

// Returns true if type_name represents a well-known type.
PROTOBUF_EXPORT bool IsWellKnownType(const std::string& type_name);

// Returns true if 'bool_string' represents a valid boolean value. Only "true",
// "false", "0" and "1" are allowed.
PROTOBUF_EXPORT bool IsValidBoolString(const std::string& bool_string);

// Returns true if "field" is a protobuf map field based on its type.
PROTOBUF_EXPORT bool IsMap(const google::protobuf::Field& field,
                           const google::protobuf::Type& type);

// Returns true if the given type has special MessageSet wire format.
bool IsMessageSetWireFormat(const google::protobuf::Type& type);

// Infinity/NaN-aware conversion to string.
PROTOBUF_EXPORT std::string DoubleAsString(double value);
PROTOBUF_EXPORT std::string FloatAsString(float value);

// Convert from int32, int64, uint32, uint64, double or float to string.
template <typename T>
std::string ValueAsString(T value) {
  return StrCat(value);
}

template <>
inline std::string ValueAsString(float value) {
  return FloatAsString(value);
}

template <>
inline std::string ValueAsString(double value) {
  return DoubleAsString(value);
}

// Converts a string to float. Unlike safe_strtof, conversion will fail if the
// value fits into double but not float (e.g., DBL_MAX).
PROTOBUF_EXPORT bool SafeStrToFloat(StringPiece str, float* value);

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_UTILITY_H__
