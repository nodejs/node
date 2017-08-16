// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-restrictive-generators

// Generators don't get sloppy-mode block-scoped function hoisting

// No hoisting to the global scope

{
  function* foo() {}
  assertEquals('function', typeof foo);
}
assertEquals('undefined', typeof foo);

// No hoisting within a function scope
(function() {
  { function* bar() {} }
  assertEquals('undefined', typeof bar);
})();

// Lexical shadowing allowed, no hoisting
(function() {
  function* x() { yield 1; }
  { function* x() { yield 2 } }
  assertEquals(1, x().next().value);
})();
