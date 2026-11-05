// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Strict mode parameter errors and arrow declaration errors should report
// the first error encountered in the parameter list rather than overwriting
// it with later errors.

assertThrows(
    () => eval('(function f(implements, eval) { "use strict"; })'),
    SyntaxError,
    "Unexpected strict mode reserved word");
assertThrows(
    () => eval('(implements, eval) => { "use strict"; }'),
    SyntaxError,
    "Unexpected strict mode reserved word");
assertThrows(
    () => eval('(function f(eval, implements) { "use strict"; })'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('(eval, implements) => { "use strict"; }'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval("([a.b], 1) => {}"),
    SyntaxError,
    "Illegal property in declaration context");
assertThrows(
    () => eval("function* g() { (b = yield a, 1) => {}; }"),
    SyntaxError,
    "Yield expression not allowed in formal parameter");
