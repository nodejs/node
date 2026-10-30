// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that unresolved variables from lazily parsed functions inside the
// parameter list of a lazily parsed arrow function are properly propagated to
// outer scopes during Scope::AnalyzePartially.

(function TestSloppyEvalLetAssignedInArrowHeadDefaultFunc() {
  eval(`
    let x = 1;
    let g = () => x;
    let f = (a = function foo() { x = 42; }) => {
      a();
    };
    for (let i = 0; i < 10000; i++) {
      assertEquals(1, g());
    }
    f();
    assertEquals(42, g());
  `);
})();

(function TestSloppyEvalArrowHeadAndOuterCapture() {
  eval(`
    let x = 1;
    let g = () => x;
    let f = (a = function foo() { b = 10; x = 99; }, b = 2) => {
      assertEquals(2, b);
      a();
      assertEquals(10, b);
    };
    for (let i = 0; i < 10000; i++) {
      assertEquals(1, g());
    }
    f();
    assertEquals(99, g());
  `);
})();

try {
  throw 1;
} catch (x) {
  var g_catch = () => x;
  var f_catch = (a = function foo() { x = 42; }) => {
    a();
  };
  for (let i = 0; i < 10000; i++) {
    assertEquals(1, g_catch());
  }
  f_catch();
  assertEquals(42, g_catch());
}
