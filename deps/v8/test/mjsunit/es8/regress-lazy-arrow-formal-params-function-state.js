// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var await = 10;
var yield = 20;

// Case 1: 'await' as binding identifier in lazy arrow formal parameters inside
// a contextless sync function nested in an async function.
(function TestAwaitBindingIdentifierInAsyncOuter() {
  async function outer() {
    let ctx;
    () => ctx;
    function sync_inner() {
      let f = await => await;
      return f(42);
    }
    return sync_inner();
  }
  assertPromiseResult(outer(), v => assertEquals(42, v));
})();

// Case 2: 'await' in default parameter initializer of lazy arrow function
// inside a contextless sync function nested in an async function.
(function TestAwaitDefaultParamInAsyncOuter() {
  async function outer() {
    let ctx;
    () => ctx;
    function sync_inner() {
      let f = (a = await / 2) => a;
      return f();
    }
    return sync_inner();
  }
  assertPromiseResult(outer(), v => assertEquals(5, v));
})();

// Case 3: 'yield' in default parameter initializer of lazy arrow function
// inside a contextless sync function nested in a generator function.
(function TestYieldDefaultParamInGeneratorOuter() {
  function* outer() {
    let ctx = 1;
    () => ctx;
    function sync_inner() {
      let f = (a = yield) => { return a; };
      return f();
    }
    return sync_inner();
  }
  assertEquals({value: 20, done: true}, outer().next());
})();

// Case 4: Both 'await' and 'yield' in lazy arrow formal parameters inside
// a contextless sync function nested in an async generator function.
(function TestAwaitAndYieldInAsyncGeneratorOuter() {
  async function* outer() {
    let ctx = 1;
    () => ctx;
    function sync_inner() {
      let f1 = await => await;
      let f2 = (a = await / 2, b = yield) => a + b;
      return f1(10) + f2();
    }
    return sync_inner();
  }
  assertPromiseResult(outer().next(), v => assertEquals({value: 35, done: true}, v));
})();
