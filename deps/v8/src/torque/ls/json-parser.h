// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_JSON_PARSER_H_
#define V8_TORQUE_LS_JSON_PARSER_H_

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/torque/ls/json.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

struct JsonParserResult {
  JsonValue value;
  base::Optional<TorqueError> error;
};

V8_EXPORT_PRIVATE JsonParserResult ParseJson(const std::string& input);

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_JSON_PARSER_H_
