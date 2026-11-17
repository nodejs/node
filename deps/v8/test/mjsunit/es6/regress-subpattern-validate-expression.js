// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Non-pattern expressions inside destructuring patterns or arrow parameter
// lists must validate inner expression errors before recording pattern or
// parameter declaration errors.

assertThrows(
    () => eval("[{a = 42}.f()] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[!{a = 42}] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[{a = 42} + 1] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[{a = 42} ? 1 : 2] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("[{a = 42}?.b] = []"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({x: {a = 42} + 1} = {})"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({a = 42}.b) => {}"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({a = 42} + 1) => {}"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("(!{a = 42}) => {}"),
    SyntaxError,
    "Invalid shorthand property initializer");
