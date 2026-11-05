// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Arrow parameter list cover-grammar extensions (rest parameters and
// trailing commas before `) =>`) are only allowed in arrow head scopes, and
// strict `eval`/`arguments` rest parameters must be classified via
// RecordStrictModeParameterError rather than failing eagerly in
// ParseBindingPattern.

assertThrows(
    () => eval("a, ...x = 1"),
    SyntaxError,
    "Unexpected token '...'");
assertThrows(
    () => eval("a, ...x, y"),
    SyntaxError,
    "Unexpected token '...'");
assertThrows(
    () => eval("if (a,) => {}"),
    SyntaxError,
    "Unexpected token ')'");
assertThrows(
    () => eval("while (a,) => {}"),
    SyntaxError,
    "Unexpected token ')'");
assertThrows(
    () => eval('"use strict"; ({a = 42}, ...eval) => {}'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('"use strict"; (1, ...eval) => {}'),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval('"use strict"; (a, ...eval)'),
    SyntaxError,
    "Unexpected token '...'");
