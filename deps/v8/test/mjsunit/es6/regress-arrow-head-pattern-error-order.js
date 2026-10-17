// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ArrowHeadParsingScope must validate both declaration and pattern errors so
// the earliest error in source order is reported, and must not overwrite
// earlier errors or validate expression errors when rest parameter parsing
// fails.

assertThrows(
    () => eval("([...a, b], 1) => {}"),
    SyntaxError,
    "Rest element must be last element");
assertThrows(
    () => eval("({...a, b}, 1) => {}"),
    SyntaxError,
    "Rest element must be last element");
assertThrows(
    () => eval("([...a, b], [c.d]) => {}"),
    SyntaxError,
    "Rest element must be last element");
assertThrows(
    () => eval("([a, a], [...b, c]) => {}"),
    SyntaxError,
    "Rest element must be last element");
assertThrows(
    () => eval("([a, a], [1]) => {}"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("({a = 42}, ...z = 1) => {}"),
    SyntaxError,
    "Rest parameter may not have a default initializer");
assertThrows(
    () => eval("({a = 42}, ...z, w) => {}"),
    SyntaxError,
    "Rest parameter must be last formal parameter");
assertThrows(
    () => eval("(1, ...z = 1) => {}"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("([a.b], ...z = 1) => {}"),
    SyntaxError,
    "Illegal property in declaration context");
assertThrows(
    () => eval("([...a, b], ...z = 1) => {}"),
    SyntaxError,
    "Rest element must be last element");
assertThrows(
    () => eval("(1, ...z, w) => {}"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("({a = 42}, ...1) => {}"),
    SyntaxError,
    "Unexpected number");
assertThrows(
    () => eval("(1, ...2) => {}"),
    SyntaxError,
    "Invalid destructuring assignment target");
