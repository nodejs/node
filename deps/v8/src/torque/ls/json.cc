// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/ls/json.h"

#include <iostream>
#include <sstream>
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

namespace {

void SerializeToString(std::stringstream& str, const JsonValue& value) {
  switch (value.tag) {
    case JsonValue::NUMBER:
      str << value.ToNumber();
      break;
    case JsonValue::STRING:
      str << StringLiteralQuote(value.ToString());
      break;
    case JsonValue::IS_NULL:
      str << "null";
      break;
    case JsonValue::BOOL:
      str << (value.ToBool() ? "true" : "false");
      break;
    case JsonValue::OBJECT: {
      str << "{";
      size_t i = 0;
      for (const auto& pair : value.ToObject()) {
        str << "\"" << pair.first << "\":";
        SerializeToString(str, pair.second);
        if (++i < value.ToObject().size()) str << ",";
      }
      str << "}";
      break;
    }
    case JsonValue::ARRAY: {
      str << "[";
      size_t i = 0;
      for (const auto& element : value.ToArray()) {
        SerializeToString(str, element);
        if (++i < value.ToArray().size()) str << ",";
      }
      str << "]";
      break;
    }
    default:
      break;
  }
}

}  // namespace

std::string SerializeToString(const JsonValue& value) {
  std::stringstream result;
  SerializeToString(result, value);
  return result.str();
}

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8
