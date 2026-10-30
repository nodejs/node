// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In ConditionalExpression (`ShortCircuitExpression ? AssignmentExpression :
// AssignmentExpression`), if the else branch is an AssignmentExpression that
// is not a ShortCircuitExpression (such as an ArrowFunction with a block body
// or a bare YieldExpression), it cannot be followed by `?` as the condition
// of another conditional expression.

assertThrows(
    () => eval("false ? 1 : () => {} ? 2 : 3"),
    SyntaxError,
    "Unexpected token '?'");
assertThrows(
    () => eval("false ? 1 : x => {} ? 2 : 3"),
    SyntaxError,
    "Unexpected token '?'");
assertThrows(
    () => eval("false ? 1 : async () => {} ? 2 : 3"),
    SyntaxError,
    "Unexpected token '?'");
assertThrows(
    () => eval("false ? 1 : async x => {} ? 2 : 3"),
    SyntaxError,
    "Unexpected token '?'");
assertThrows(
    () => eval("function* g() { return false ? 1 : yield\n ? 2 : 3; }"),
    SyntaxError,
    "Unexpected token '?'");
assertThrows(
    () => eval("true ? () => {} : () => {} ? 2 : 3"),
    SyntaxError,
    "Unexpected token '?'");
