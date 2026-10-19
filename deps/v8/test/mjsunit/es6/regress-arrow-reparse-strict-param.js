// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When a lazy top-level arrow function aborts preparsing due to an
// unidentifiable error in its body and reparses the arrow head, strict
// parameter errors in the head must still be preserved and reported first.

assertThrows(
    () => eval('let f = (eval) => { "use strict"; let x; let x; };'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('let f = eval => { "use strict"; let x; let x; };'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('let f = (arguments) => { "use strict"; let x; let x; };'),
    SyntaxError,
    "Unexpected eval or arguments in strict mode");
assertThrows(
    () => eval('let f = (implements) => { "use strict"; function g(a, a) {} };'),
    SyntaxError,
    "Unexpected strict mode reserved word");
