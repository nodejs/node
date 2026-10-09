// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Expression errors inside an invalid assignment or count-operation LHS
// must be validated before reporting or rewriting the invalid reference.

assertThrows(
    () => eval("+({a = 42}) = 1"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("+{a = 42} = 1"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("++{a = 42}"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({a = 42}++)"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({a = 42}.f() &&= 1)"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({a = 42}.f`` = 1)"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[{a = 42}.f() = 1] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
