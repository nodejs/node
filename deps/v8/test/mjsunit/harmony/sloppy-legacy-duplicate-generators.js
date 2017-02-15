// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-restrictive-generators

// In legacy mode, generators get sloppy-mode block-scoped function hoisting

// Hoisting to the global scope

{
  function* foo() {}
  assertEquals('function', typeof foo);
}
//assertEquals('function', typeof foo);

// Hoisting within a function scope
(function() {
  { function* bar() {} }
  assertEquals('function', typeof bar);
})();

// Lexical shadowing allowed; hoisting happens
(function() {
  function* x() { yield 1; }
  { function* x() { yield 2 } }
  assertEquals(2, x().next().value);
})();

// Duplicates allowed
(function() {
  function* y() { yield 1; }
  function* y() { yield 2 }
  assertEquals(2, y().next().value);
})();

// Functions and generators may duplicate each other
(function() {
  function* z() { yield 1; }
  function z() { return 2 }
  assertEquals(2, z());

  function a() { return 1; }
  function* a() { yield 2 }
  assertEquals(2, a().next().value);
})();

// In strict mode, none of this happens

(function() {
  'use strict';

  { function* bar() {} }
  assertEquals('undefined', typeof bar);

  // Lexical shadowing allowed; hoisting happens
  function* x() { yield 1; }
  { function* x() { yield 2 } }
  assertEquals(1, x().next().value);
})();
