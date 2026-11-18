// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Strict-mode octal literals and octal escape sequences inside a class or
// function body must be rejected even when the token immediately after the
// closing `}` in an outer sloppy scope is an octal literal or escape, or
// when a lazy function or arrow function has a later declaration conflict.

assertThrows(
    () => eval("class C { static x = 010; }\n020;"),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval("class C { static { 010; } }\n020;"),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval("class C { x = 010; }\n020;"),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('class C { static x = "\\01"; }\n020;'),
    SyntaxError,
    "Octal escape sequences are not allowed in strict mode.");
assertThrows(
    () => eval('!function() { "use strict"; return 010; }\n020;'),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('!function() { "use strict"; return "\\01"; }\n020;'),
    SyntaxError,
    "Octal escape sequences are not allowed in strict mode.");
assertThrows(
    () => eval('function f() { "use strict"; return 010; }\n020;'),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('function outer() { function f() { "use strict"; return 010; }\n020; }'),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('function outer() { () => { "use strict"; return 010; }\n020; }'),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('(() => { "use strict"; return 010; })\n020;'),
    SyntaxError,
    "Octal literals are not allowed in strict mode.");
assertThrows(
    () => eval('() => { "\\01"; "use strict"; var x; let x; }'),
    SyntaxError,
    "Octal escape sequences are not allowed in strict mode.");
assertThrows(
    () => eval('function f() { "\\01"; "use strict"; { let x; { var x; } } }'),
    SyntaxError,
    "Octal escape sequences are not allowed in strict mode.");
