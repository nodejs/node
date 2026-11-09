// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Object spread/rest properties must validate inner expression errors when
// the operand is not a valid reference expression, and must only record
// kElementAfterRest when followed by a comma.

assertThrows(
    () => eval("({...{a = 42}} = {})"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({...[{a = 42}]} = {})"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({...{a = 42}}) => {}"),
    SyntaxError,
    "Invalid shorthand property initializer");
assertThrows(
    () => eval("({...1} = {})"),
    SyntaxError,
    "`...` must be followed by an assignable reference in assignment contexts");
assertThrows(
    () => eval("({...[1]} = {})"),
    SyntaxError,
    "`...` must be followed by an assignable reference in assignment contexts");
assertThrows(
    () => eval("let { ...a b } = {}"),
    SyntaxError,
    "Unexpected identifier 'b'");
assertThrows(
    () => eval("let { ...a : 1 } = {}"),
    SyntaxError,
    "Unexpected token ':'");
assertThrows(
    () => eval("function f({ ...a : 1 }) {}"),
    SyntaxError,
    "Unexpected token ':'");
