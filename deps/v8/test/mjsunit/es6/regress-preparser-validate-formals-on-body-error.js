// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Lazy functions must validate formal parameters and propagate body strict
// mode even when the function body has a syntax error, matching eager
// parsing and reporting the earlier parameter error first.

assertThrows(
    () => eval('function f(eval) { "use strict"; break; }'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('function f(a, a) { "use strict"; break; }'),
    SyntaxError,
    "Duplicate parameter name not allowed in this context");
assertThrows(
    () => eval('function f(eval, a = 1) { "use strict"; }'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('let f = (eval, a = 1) => { "use strict"; };'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('let f = (eval) => { "use strict"; break; };'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
