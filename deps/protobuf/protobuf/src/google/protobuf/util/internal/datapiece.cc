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

#include <google/protobuf/util/internal/datapiece.h>

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/type.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/internal/utility.h>

#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/mathlimits.h>
#include <google/protobuf/stubs/mathutil.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

;
;
;
using util::Status;
using util::StatusOr;
using util::error::Code;

namespace {

inline Status InvalidArgument(StringPiece value_str) {
  return Status(util::error::INVALID_ARGUMENT, value_str);
}

template <typename To, typename From>
StatusOr<To> ValidateNumberConversion(To after, From before) {
  if (after == before &&
      MathUtil::Sign<From>(before) == MathUtil::Sign<To>(after)) {
    return after;
  } else {
    return InvalidArgument(std::is_integral<From>::value
                               ? ValueAsString(before)
                               : std::is_same<From, double>::value
                                     ? DoubleAsString(before)
                                     : FloatAsString(before));
  }
}

// For general conversion between
//     int32, int64, uint32, uint64, double and float
// except conversion between double and float.
template <typename To, typename From>
StatusOr<To> NumberConvertAndCheck(From before) {
  if (std::is_same<From, To>::value) return before;

  To after = static_cast<To>(before);
  return ValidateNumberConversion(after, before);
}

// For conversion to integer types (int32, int64, uint32, uint64) from floating
// point types (double, float) only.
template <typename To, typename From>
StatusOr<To> FloatingPointToIntConvertAndCheck(From before) {
  if (std::is_same<From, To>::value) return before;

  To after = static_cast<To>(before);
  return ValidateNumberConversion(after, before);
}

// For conversion between double and float only.
StatusOr<double> FloatToDouble(float before) {
  // Casting float to double should just work as double has more precision
  // than float.
  return static_cast<double>(before);
}

StatusOr<float> DoubleToFloat(double before) {
  if (MathLimits<double>::IsNaN(before)) {
    return std::numeric_limits<float>::quiet_NaN();
  } else if (!MathLimits<double>::IsFinite(before)) {
    // Converting a double +inf/-inf to float should just work.
    return static_cast<float>(before);
  } else if (before > std::numeric_limits<float>::max() ||
             before < -std::numeric_limits<float>::max()) {
    // Double value outside of the range of float.
    return InvalidArgument(DoubleAsString(before));
  } else {
    return static_cast<float>(before);
  }
}

}  // namespace

StatusOr<int32> DataPiece::ToInt32() const {
  if (type_ == TYPE_STRING) return StringToNumber<int32>(safe_strto32);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<int32, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<int32, float>(float_);

  return GenericConvert<int32>();
}

StatusOr<uint32> DataPiece::ToUint32() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<uint32>(safe_strtou32);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<uint32, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<uint32, float>(float_);

  return GenericConvert<uint32>();
}

StatusOr<int64> DataPiece::ToInt64() const {
  if (type_ == TYPE_STRING) return StringToNumber<int64>(safe_strto64);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<int64, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<int64, float>(float_);

  return GenericConvert<int64>();
}

StatusOr<uint64> DataPiece::ToUint64() const {
  if (type_ == TYPE_STRING)
    return StringToNumber<uint64>(safe_strtou64);

  if (type_ == TYPE_DOUBLE)
    return FloatingPointToIntConvertAndCheck<uint64, double>(double_);

  if (type_ == TYPE_FLOAT)
    return FloatingPointToIntConvertAndCheck<uint64, float>(float_);

  return GenericConvert<uint64>();
}

StatusOr<double> DataPiece::ToDouble() const {
  if (type_ == TYPE_FLOAT) {
    return FloatToDouble(float_);
  }
  if (type_ == TYPE_STRING) {
    if (str_ == "Infinity") return std::numeric_limits<double>::infinity();
    if (str_ == "-Infinity") return -std::numeric_limits<double>::infinity();
    if (str_ == "NaN") return std::numeric_limits<double>::quiet_NaN();
    StatusOr<double> value = StringToNumber<double>(safe_strtod);
    if (value.ok() && !MathLimits<double>::IsFinite(value.ValueOrDie())) {
      // safe_strtod converts out-of-range values to +inf/-inf, but we want
      // to report them as errors.
      return InvalidArgument(StrCat("\"", str_, "\""));
    } else {
      return value;
    }
  }
  return GenericConvert<double>();
}

StatusOr<float> DataPiece::ToFloat() const {
  if (type_ == TYPE_DOUBLE) {
    return DoubleToFloat(double_);
  }
  if (type_ == TYPE_STRING) {
    if (str_ == "Infinity") return std::numeric_limits<float>::infinity();
    if (str_ == "-Infinity") return -std::numeric_limits<float>::infinity();
    if (str_ == "NaN") return std::numeric_limits<float>::quiet_NaN();
    // SafeStrToFloat() is used instead of safe_strtof() because the later
    // does not fail on inputs like SimpleDtoa(DBL_MAX).
    return StringToNumber<float>(SafeStrToFloat);
  }
  return GenericConvert<float>();
}

StatusOr<bool> DataPiece::ToBool() const {
  switch (type_) {
    case TYPE_BOOL:
      return bool_;
    case TYPE_STRING:
      return StringToNumber<bool>(safe_strtob);
    default:
      return InvalidArgument(
          ValueAsStringOrDefault("Wrong type. Cannot convert to Bool."));
  }
}

StatusOr<std::string> DataPiece::ToString() const {
  switch (type_) {
    case TYPE_STRING:
      return std::string(str_);
    case TYPE_BYTES: {
      std::string base64;
      Base64Escape(str_, &base64);
      return base64;
    }
    default:
      return InvalidArgument(
          ValueAsStringOrDefault("Cannot convert to string."));
  }
}

std::string DataPiece::ValueAsStringOrDefault(
    StringPiece default_string) const {
  switch (type_) {
    case TYPE_INT32:
      return StrCat(i32_);
    case TYPE_INT64:
      return StrCat(i64_);
    case TYPE_UINT32:
      return StrCat(u32_);
    case TYPE_UINT64:
      return StrCat(u64_);
    case TYPE_DOUBLE:
      return DoubleAsString(double_);
    case TYPE_FLOAT:
      return FloatAsString(float_);
    case TYPE_BOOL:
      return SimpleBtoa(bool_);
    case TYPE_STRING:
      return StrCat("\"", str_.ToString(), "\"");
    case TYPE_BYTES: {
      std::string base64;
      WebSafeBase64Escape(str_, &base64);
      return StrCat("\"", base64, "\"");
    }
    case TYPE_NULL:
      return "null";
    default:
      return std::string(default_string);
  }
}

StatusOr<std::string> DataPiece::ToBytes() const {
  if (type_ == TYPE_BYTES) return str_.ToString();
  if (type_ == TYPE_STRING) {
    std::string decoded;
    if (!DecodeBase64(str_, &decoded)) {
      return InvalidArgument(ValueAsStringOrDefault("Invalid data in input."));
    }
    return decoded;
  } else {
    return InvalidArgument(ValueAsStringOrDefault(
        "Wrong type. Only String or Bytes can be converted to Bytes."));
  }
}

StatusOr<int> DataPiece::ToEnum(const google::protobuf::Enum* enum_type,
                                bool use_lower_camel_for_enums,
                                bool case_insensitive_enum_parsing,
                                bool ignore_unknown_enum_values,
                                bool* is_unknown_enum_value) const {
  if (type_ == TYPE_NULL) return google::protobuf::NULL_VALUE;

  if (type_ == TYPE_STRING) {
    // First try the given value as a name.
    std::string enum_name = std::string(str_);
    const google::protobuf::EnumValue* value =
        FindEnumValueByNameOrNull(enum_type, enum_name);
    if (value != nullptr) return value->number();

    // Check if int version of enum is sent as string.
    StatusOr<int32> int_value = ToInt32();
    if (int_value.ok()) {
      if (const google::protobuf::EnumValue* enum_value =
              FindEnumValueByNumberOrNull(enum_type, int_value.ValueOrDie())) {
        return enum_value->number();
      }
    }

    // Next try a normalized name.
    bool should_normalize_enum =
        case_insensitive_enum_parsing || use_lower_camel_for_enums;
    if (should_normalize_enum) {
      for (std::string::iterator it = enum_name.begin(); it != enum_name.end();
           ++it) {
        *it = *it == '-' ? '_' : ascii_toupper(*it);
      }
      value = FindEnumValueByNameOrNull(enum_type, enum_name);
      if (value != nullptr) return value->number();
    }

    // If use_lower_camel_for_enums is true try with enum name without
    // underscore. This will also accept camel case names as the enum_name has
    // been normalized before.
    if (use_lower_camel_for_enums) {
      value = FindEnumValueByNameWithoutUnderscoreOrNull(enum_type, enum_name);
      if (value != nullptr) return value->number();
    }

    // If ignore_unknown_enum_values is true an unknown enum value is ignored.
    if (ignore_unknown_enum_values) {
      *is_unknown_enum_value = true;
      return enum_type->enumvalue(0).number();
    }
  } else {
    // We don't need to check whether the value is actually declared in the
    // enum because we preserve unknown enum values as well.
    return ToInt32();
  }
  return InvalidArgument(
      ValueAsStringOrDefault("Cannot find enum with given value."));
}

template <typename To>
StatusOr<To> DataPiece::GenericConvert() const {
  switch (type_) {
    case TYPE_INT32:
      return NumberConvertAndCheck<To, int32>(i32_);
    case TYPE_INT64:
      return NumberConvertAndCheck<To, int64>(i64_);
    case TYPE_UINT32:
      return NumberConvertAndCheck<To, uint32>(u32_);
    case TYPE_UINT64:
      return NumberConvertAndCheck<To, uint64>(u64_);
    case TYPE_DOUBLE:
      return NumberConvertAndCheck<To, double>(double_);
    case TYPE_FLOAT:
      return NumberConvertAndCheck<To, float>(float_);
    default:  // TYPE_ENUM, TYPE_STRING, TYPE_CORD, TYPE_BOOL
      return InvalidArgument(ValueAsStringOrDefault(
          "Wrong type. Bool, Enum, String and Cord not supported in "
          "GenericConvert."));
  }
}

template <typename To>
StatusOr<To> DataPiece::StringToNumber(bool (*func)(StringPiece,
                                                    To*)) const {
  if (str_.size() > 0 && (str_[0] == ' ' || str_[str_.size() - 1] == ' ')) {
    return InvalidArgument(StrCat("\"", str_, "\""));
  }
  To result;
  if (func(str_, &result)) return result;
  return InvalidArgument(StrCat("\"", string(str_), "\""));
}

bool DataPiece::DecodeBase64(StringPiece src, std::string* dest) const {
  // Try web-safe decode first, if it fails, try the non-web-safe decode.
  if (WebSafeBase64Unescape(src, dest)) {
    if (use_strict_base64_decoding_) {
      // In strict mode, check if the escaped version gives us the same value as
      // unescaped.
      std::string encoded;
      // WebSafeBase64Escape does no padding by default.
      WebSafeBase64Escape(*dest, &encoded);
      // Remove trailing padding '=' characters before comparison.
      StringPiece src_no_padding = StringPiece(src).substr(
          0, HasSuffixString(src, "=") ? src.find_last_not_of('=') + 1
                                      : src.length());
      return encoded == src_no_padding;
    }
    return true;
  }

  if (Base64Unescape(src, dest)) {
    if (use_strict_base64_decoding_) {
      std::string encoded;
      Base64Escape(
          reinterpret_cast<const unsigned char*>(dest->data()), dest->length(),
          &encoded, false);
      StringPiece src_no_padding = StringPiece(src).substr(
          0, HasSuffixString(src, "=") ? src.find_last_not_of('=') + 1
                                      : src.length());
      return encoded == src_no_padding;
    }
    return true;
  }

  return false;
}

void DataPiece::InternalCopy(const DataPiece& other) {
  type_ = other.type_;
  use_strict_base64_decoding_ = other.use_strict_base64_decoding_;
  switch (type_) {
    case TYPE_INT32:
    case TYPE_INT64:
    case TYPE_UINT32:
    case TYPE_UINT64:
    case TYPE_DOUBLE:
    case TYPE_FLOAT:
    case TYPE_BOOL:
    case TYPE_ENUM:
    case TYPE_NULL:
    case TYPE_BYTES:
    case TYPE_STRING: {
      str_ = other.str_;
      break;
    }
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
