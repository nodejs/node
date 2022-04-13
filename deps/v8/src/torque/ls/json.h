// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_JSON_H_
#define V8_TORQUE_LS_JSON_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/base/logging.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

struct JsonValue;

using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
 public:
  enum { OBJECT, ARRAY, STRING, NUMBER, BOOL, IS_NULL } tag;

  // JsonValues can only be moved, not copied.
  JsonValue() V8_NOEXCEPT = default;
  constexpr JsonValue(const JsonValue& other) = delete;
  JsonValue& operator=(const JsonValue& other) = delete;

  JsonValue(JsonValue&& other) V8_NOEXCEPT = default;
  JsonValue& operator=(JsonValue&& other) V8_NOEXCEPT = default;

  static JsonValue From(double number) {
    JsonValue result;
    result.tag = JsonValue::NUMBER;
    result.number_ = number;
    return result;
  }

  static JsonValue From(JsonObject object) {
    JsonValue result;
    result.tag = JsonValue::OBJECT;
    result.object_ = std::make_unique<JsonObject>(std::move(object));
    return result;
  }

  static JsonValue From(bool b) {
    JsonValue result;
    result.tag = JsonValue::BOOL;
    result.flag_ = b;
    return result;
  }

  static JsonValue From(const std::string& string) {
    JsonValue result;
    result.tag = JsonValue::STRING;
    result.string_ = string;
    return result;
  }

  static JsonValue From(JsonArray array) {
    JsonValue result;
    result.tag = JsonValue::ARRAY;
    result.array_ = std::make_unique<JsonArray>(std::move(array));
    return result;
  }

  static JsonValue JsonNull() {
    JsonValue result;
    result.tag = JsonValue::IS_NULL;
    return result;
  }

  bool IsNumber() const { return tag == NUMBER; }
  double ToNumber() const {
    CHECK(IsNumber());
    return number_;
  }

  bool IsBool() const { return tag == BOOL; }
  bool ToBool() const {
    CHECK(IsBool());
    return flag_;
  }

  bool IsString() const { return tag == STRING; }
  const std::string& ToString() const {
    CHECK(IsString());
    return string_;
  }

  bool IsObject() const { return object_ && tag == OBJECT; }
  const JsonObject& ToObject() const {
    CHECK(IsObject());
    return *object_;
  }
  JsonObject& ToObject() {
    CHECK(IsObject());
    return *object_;
  }

  bool IsArray() const { return array_ && tag == ARRAY; }
  const JsonArray& ToArray() const {
    CHECK(IsArray());
    return *array_;
  }
  JsonArray& ToArray() {
    CHECK(IsArray());
    return *array_;
  }

 private:
  double number_ = 0;
  bool flag_ = false;
  std::string string_;
  std::unique_ptr<JsonObject> object_;
  std::unique_ptr<JsonArray> array_;
};

std::string SerializeToString(const JsonValue& value);

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_JSON_H_
