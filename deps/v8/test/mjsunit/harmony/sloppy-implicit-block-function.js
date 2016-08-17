// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-restrictive-declarations

// ES#sec-functiondeclarations-in-ifstatement-statement-clauses
// Annex B 3.4 FunctionDeclarations in IfStatement Statement Clauses
// In sloppy mode, function declarations in if statements act like
// they have a block around them. Prohibited in strict mode.
(function() {
  assertEquals(undefined, f);
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

// For legacy reasons, we also support these types of semantics as
// the body of a for or with statement.
(function() {
  for (;false;) function f() { };
  assertEquals(undefined, f);
})();

(function() {
  for (var x in {}) function f() { };
  assertEquals(undefined, f);
})();

(function() {
  var x;
  for (x in {}) function f() { };
  assertEquals(undefined, f);
})();

(function() {
  for (var i = 0; i < 1; i++) function f() { };
  assertEquals('function', typeof f);
})();

(function() {
  for (var x in {a: 1}) function f() { };
  assertEquals('function', typeof f);
})();

(function() {
  var x;
  for (x in {a: 1}) function f() { };
  assertEquals('function', typeof f);
})();

(function() {
  with ({}) function f() { };
  assertEquals('function', typeof f);
})();

(function() {
  do function f() {} while (0);
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
