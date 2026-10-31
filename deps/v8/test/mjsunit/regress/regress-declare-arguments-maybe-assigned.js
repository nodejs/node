// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test eager function with `var arguments = 42`.
(function testEagerVarArguments() {
  let g = () => arguments;
  for (let i = 0; i < 10000; i++) {
    assertEquals('object', typeof g());
  }
  var arguments = 42;
  assertEquals(42, g());
})();

// Test lazy function with `var arguments = 42`.
function testLazyVarArguments() {
  let g = () => arguments;
  for (let i = 0; i < 10000; i++) {
    assertEquals('object', typeof g());
  }
  var arguments = 42;
  assertEquals(42, g());
}
testLazyVarArguments();

// Test Annex B block-scoped function declaration `{ function arguments() {} }`.
function testAnnexBBlockFunctionArguments() {
  let g = () => arguments;
  for (let i = 0; i < 10000; i++) {
    assertEquals('object', typeof g());
  }
  {
    function arguments() {
      return 'annex-b';
    }
  }
  assertEquals('function', typeof g());
  assertEquals('annex-b', g()());
}
testAnnexBBlockFunctionArguments();

// Test destructuring `var {arguments = 42} = {}`.
function testDestructuringVarArguments() {
  let g = () => arguments;
  for (let i = 0; i < 10000; i++) {
    assertEquals('object', typeof g());
  }
  var {arguments = 42} = {};
  assertEquals(42, g());
}
testDestructuringVarArguments();
