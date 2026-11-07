// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

assertThrows(() => eval("((eval, a = (1)) => eval)(42)"), SyntaxError);
assertThrows(() => eval("((arguments, a = (1)) => arguments)(42)"), SyntaxError);
assertThrows(() => eval("((eval, a = () => {}) => eval)(42)"), SyntaxError);
assertThrows(() => eval("((eval, a = async(1)) => eval)(42)"), SyntaxError);
assertThrows(() => eval("(async (eval, a = (1)) => eval)(42)"), SyntaxError);
assertThrows(() => eval("(async (arguments, a = (1)) => arguments)(42)"), SyntaxError);

// Also test lazy compilation (PreParser).
assertThrows(() => eval("function f() { const g = (eval, a = (1)) => eval; }"), SyntaxError);
assertThrows(() => eval("function f() { const g = async (eval, a = (1)) => eval; }"), SyntaxError);

// Valid strict-mode arrow functions with parenthesized eval/arguments in default
// initializers or comma expressions must still succeed.
assertEquals(typeof ((a = (eval)) => a)(), "function");
assertEquals((eval, (a = (1)) => a)(), 1);
