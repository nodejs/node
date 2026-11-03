// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TORQUE_PARSER_H_
#define V8_TORQUE_TORQUE_PARSER_H_

#include "src/torque/ast.h"

namespace v8 {
namespace internal {
namespace torque {

// Adds the parsed input to {CurrentAst}
void ParseTorque(const std::string& input);

// Parse a standalone Torque type or expression without adding a declaration
// to CurrentAst. Used for the Torque source in V8_TQ_* annotation arguments.
TypeExpression* ParseTorqueTypeExpression(const std::string& input);
Expression* ParseTorqueExpression(const std::string& input);
// Parse a sequence of class field declarations, as carried by
// V8_TQ_TAIL_SECTIONS for a tail Torque splits into several indexed
// sections.
std::vector<ClassFieldExpression> ParseTorqueClassFields(
    const std::string& input);

Expression* MakeCall(Identifier* callee,
                     const std::vector<TypeExpression*>& generic_arguments,
                     const std::vector<Expression*>& arguments,
                     const std::vector<Statement*>& otherwise);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_PARSER_H_
