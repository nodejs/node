// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TORQUE_PARSER_H_
#define V8_TORQUE_TORQUE_PARSER_H_

#include "src/torque/ast.h"

namespace v8 {
namespace internal {
namespace torque {

DECLARE_CONTEXTUAL_VARIABLE(CurrentAst, Ast);

// Adds the parsed input to {CurrentAst}
void ParseTorque(const std::string& input);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_PARSER_H_
