// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// YieldExpression operands are unconditionally AssignmentExpressions and
// should validate expression errors before outer pattern validation.

assertThrows(
    () => eval("function* g() { [yield {a = 42}] = []; }"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("function* g() { [yield* {a = 42}] = []; }"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("function* g() { ({x: yield {a = 42}} = {}); }"),
    SyntaxError,
    "Invalid shorthand property initializer");
