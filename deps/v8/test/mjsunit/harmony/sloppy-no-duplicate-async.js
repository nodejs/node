// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-await

// Async functions don't get sloppy-mode block-scoped function hoisting

// No hoisting to the global scope

{
  async function foo() {}
  assertEquals('function', typeof foo);
}
assertEquals('undefined', typeof foo);

// No hoisting within a function scope
(function() {
  { async function bar() {} }
  assertEquals('undefined', typeof bar);
})();

// Lexical shadowing allowed, no hoisting
(function() {
  var y;
  async function x() { y = 1; }
  { async function x() { y = 2; } }
  x();
  assertEquals(1, y);
})();
