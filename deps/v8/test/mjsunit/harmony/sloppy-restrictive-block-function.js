// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-restrictive-declarations

// ES#sec-functiondeclarations-in-ifstatement-statement-clauses
// Annex B 3.4 FunctionDeclarations in IfStatement Statement Clauses
// In sloppy mode, function declarations in if statements act like
// they have a block around them. Prohibited in strict mode.
(function() {
  if (false) function f() { };
  assertEquals(undefined, f);
})();

(function() {
  assertEquals(undefined, f);
  if (true) function f() { };
  assertEquals('function', typeof f);
})();

(function() {
  assertEquals(undefined, f);
  if (true) {} else function f() { };
  assertEquals(undefined, f);
})();

(function() {
  assertEquals(undefined, f);
  if (false) {} else function f() { };
  assertEquals('function', typeof f);
})();

// Labeled function declarations undergo the same hoisting/FiB semantics as if
// they were unalbeled.
(function() {
  function bar() {
    return f;
    x: function f() {}
  }
  assertEquals('function', typeof bar());
})();

(function() {
  function bar() {
    return f;
    {
      x: function f() {}
    }
  }
  assertEquals(undefined, bar());
})();
