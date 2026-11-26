// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

class DerivedFunction extends Function {}

// Eager deopt when CallSelf enters Maglev code with a distinct closure sharing
// the same SharedFunctionInfo whose FeedbackCell holds a
// ClosureFeedbackCellArray rather than a FeedbackVector.
(function testEagerDeoptDistinctFeedbackCell() {
  const body = `return function rec(call_other, x) {
    if (call_other) return target(false, x);
    return x + 1;
  };`;
  const make1 = new DerivedFunction('target', body);
  const make2 = new DerivedFunction('target', body);

  const unused2 = make2(null);
  const f2 = make2(null);
  assertNotEquals(unused2, f2);

  const unused1 = make1(null);
  const f1 = make1(f2);
  assertNotEquals(unused1, f1);

  %PrepareFunctionForOptimization(f1);
  assertEquals(1, f1(true, 0));
  assertEquals(2, f1(false, 1));

  %OptimizeMaglevOnNextCall(f1);
  assertEquals(3, f1(false, 2));
  assertEquals('str1', f1(true, 'str'));
})();

// Lazy deopt when CallSelf enters Maglev code with a distinct closure sharing
// the same SharedFunctionInfo whose FeedbackCell lacks a FeedbackVector.
globalThis.lazyState = 1;
(function testLazyDeoptDistinctFeedbackCell() {
  const body = `
    if (next !== null) {
      return next(null, callback) + globalThis.lazyState;
    }
    if (callback !== null) {
      callback();
    }
    return globalThis.lazyState;
  `;
  const createFn = (ctor) =>
      Reflect.construct(Function, ['next', 'callback', body], ctor);

  const fn1 = createFn(Function);
  const fn1Sibling = createFn(Function);
  const fn2 = createFn(DerivedFunction);
  assertNotEquals(fn1, fn1Sibling);

  const noop = () => {};
  %PrepareFunctionForOptimization(fn1);
  assertEquals(2, fn1(fn2, noop));
  assertEquals(1, fn1(null, noop));

  %OptimizeMaglevOnNextCall(fn1);
  assertEquals(1, fn1(null, noop));
  assertEquals(3, fn1(fn2, () => {
    globalThis.lazyState = 1.5;
  }));
})();

// Eager deopt when CallSelf enters Maglev code with a never-called closure
// whose FeedbackCell value is still undefined (even without lazy feedback
// allocation).
(function testUncompiledFeedbackCell() {
  function invoke(fn, do_call, x) {
    if (do_call) return fn(0, x);
    return 0;
  }

  const body = `return function rec(mode, x) {
    const fn = globalThis.neverCalledTarget;
    if (mode >= 1) return invoke(fn, mode === 2, x);
    return x + 1;
  };`;
  const make1 = new DerivedFunction('invoke', body);
  const make2 = new DerivedFunction('invoke', body);

  globalThis.neverCalledTarget = make2(invoke);

  %PrepareFunctionForOptimization(invoke);
  assertEquals(2, invoke((m, v) => v + 1, true, 1));
  assertEquals(3, invoke((m, v) => v + 2, true, 1));
  assertEquals(0, invoke((m, v) => v + 1, false, 1));

  const unused1 = make1(invoke);
  const f1 = make1(invoke);
  assertNotEquals(unused1, f1);

  %PrepareFunctionForOptimization(f1);
  assertEquals(0, f1(1, 1));
  assertEquals(2, f1(0, 1));

  %OptimizeMaglevOnNextCall(f1);
  assertEquals(3, f1(0, 2));
  assertEquals('str1', f1(2, 'str'));
})();
