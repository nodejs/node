// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bracketed property keys and template literal interpolations are always
// expressions, never binding patterns or arrow parameter lists.

assertThrows(
    () => eval("let [a, b[a]] = [];"),
    SyntaxError,
    "Illegal property in declaration context");
assertThrows(
    () => eval("let [b[let]] = [];"),
    SyntaxError,
    "Illegal property in declaration context");
assertThrows(
    () => eval("let [a, b?.[a]] = [];"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("let [a, tag`${a}`] = [];"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("let [tag`${let}`] = [];"),
    SyntaxError,
    "Invalid destructuring assignment target");
assertThrows(
    () => eval("[obj[{a = 42}].f()] = [];"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[obj?.[{a = 42}].f()] = [];"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[`${{a = 42}}`] = [];"),
    SyntaxError,
    "Invalid shorthand property initializer");
