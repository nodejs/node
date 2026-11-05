// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_JSON_PARSER_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_JSON_PARSER_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace redline {

// A parsed JSON document (configuration files, saved state). Small and
// strict: no comments, no trailing commas, numbers are doubles.
class JsonValue {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  JsonValue() = default;
  static JsonValue Null() { return JsonValue(); }

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::kNull; }
  bool is_bool() const { return type_ == Type::kBool; }
  bool is_number() const { return type_ == Type::kNumber; }
  bool is_string() const { return type_ == Type::kString; }
  bool is_array() const { return type_ == Type::kArray; }
  bool is_object() const { return type_ == Type::kObject; }

  bool as_bool() const { return bool_; }
  double as_number() const { return number_; }
  int64_t as_int() const {
    if (!std::isfinite(number_)) {
      return 0;
    }
    if (number_ >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      return std::numeric_limits<int64_t>::max();
    }
    if (number_ <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
      return std::numeric_limits<int64_t>::min();
    }
    return static_cast<int64_t>(number_);
  }
  const std::string& as_string() const { return string_; }
  const std::vector<JsonValue>& as_array() const { return array_; }
  const std::vector<std::pair<std::string, JsonValue>>& as_object() const {
    return object_;
  }

  // Object member lookup; returns a null value for missing keys.
  const JsonValue& Get(std::string_view key) const;
  bool Has(std::string_view key) const;
  // Typed getters with defaults (also used for missing keys).
  double GetNumber(std::string_view key, double fallback) const;
  int64_t GetInt(std::string_view key, int64_t fallback) const;
  bool GetBool(std::string_view key, bool fallback) const;
  std::string GetString(std::string_view key, std::string_view fallback) const;

  // Parses `text`; on error returns false and sets `error` ("line:col: msg").
  static bool Parse(std::string_view text, JsonValue* out, std::string* error);

  // Serializes back to compact JSON (for round-trip tests and dumps).
  std::string ToString() const;

 private:
  friend class JsonParser;
  Type type_ = Type::kNull;
  bool bool_ = false;
  double number_ = 0;
  std::string string_;
  std::vector<JsonValue> array_;
  std::vector<std::pair<std::string, JsonValue>> object_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_JSON_PARSER_H_
