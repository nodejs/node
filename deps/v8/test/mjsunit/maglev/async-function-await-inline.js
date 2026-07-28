// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

(function FastPathFulfilled() {
  async function f(p) {
    return await p;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(Promise.resolve(1)), r => assertEquals(1, r));
  assertPromiseResult(f(Promise.resolve(2)), r => assertEquals(2, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(Promise.resolve(42)), r => {
    assertEquals(42, r);
    assertOptimized(f);
  });
})();

(function FastPathUndefinedValue() {
  async function f(p) {
    return await p;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(Promise.resolve()), r => assertEquals(undefined, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(Promise.resolve()), r => {
    assertEquals(undefined, r);
    assertOptimized(f);
  });
})();

(function FastPathMicrotaskOrder() {
  const log = [];
  async function f(p) {
    log.push('pre');
    const v = await p;
    log.push('post:' + v);
    return v;
  }
  %PrepareFunctionForOptimization(f);
  f(Promise.resolve('a'));
  %OptimizeFunctionOnNextCall(f);
  const ret = f(Promise.resolve('b'));
  assertEquals(['pre', 'pre'], log);
  assertPromiseResult(ret, r => {
    assertEquals('b', r);
    assertEquals(['pre', 'pre', 'post:a', 'post:b'], log);
  });
})();

(function SlowPathSmi() {
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(7), r => assertEquals(7, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(13), r => {
    assertEquals(13, r);
    assertOptimized(f);
  });
})();

(function SlowPathPlainObject() {
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  const o1 = {x: 1};
  const o2 = {x: 2};
  assertPromiseResult(f(o1), r => assertSame(o1, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(o2), r => {
    assertSame(o2, r);
    assertOptimized(f);
  });
})();

(function SlowPathThenable() {
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f({then(resolve) { resolve(11); }}),
                      r => assertEquals(11, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f({then(resolve) { resolve(22); }}), r => {
    assertEquals(22, r);
    assertOptimized(f);
  });
})();

(function SlowPathPromiseSubclass() {
  class Sub extends Promise {}
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(Sub.resolve(3)), r => assertEquals(3, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(Sub.resolve(4)), r => {
    assertEquals(4, r);
    assertOptimized(f);
  });
})();

(function SlowPathPending() {
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  let resolve1;
  const p1 = new Promise(r => { resolve1 = r; });
  const ret1 = f(p1);
  resolve1(5);
  assertPromiseResult(ret1, r => assertEquals(5, r));
  %OptimizeFunctionOnNextCall(f);
  let resolve2;
  const p2 = new Promise(r => { resolve2 = r; });
  const ret2 = f(p2);
  resolve2(6);
  assertPromiseResult(ret2, r => {
    assertEquals(6, r);
    assertOptimized(f);
  });
})();

(function SlowPathRejected() {
  async function f(v) {
    try {
      return await v;
    } catch (e) {
      return 'caught:' + e;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(Promise.reject('boom1')),
                      r => assertEquals('caught:boom1', r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(Promise.reject('boom2')), r => {
    assertEquals('caught:boom2', r);
  });
})();

(function FastPathAwaitsInLoop() {
  async function f(n) {
    let sum = 0;
    for (let i = 0; i < n; i++) {
      sum += await Promise.resolve(i);
    }
    return sum;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(3), r => assertEquals(0 + 1 + 2, r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(5), r => assertEquals(0 + 1 + 2 + 3 + 4, r));
})();

(function FastPathReturnValueIsOuterPromise() {
  async function f(p) {
    return await p;
  }
  %PrepareFunctionForOptimization(f);
  f(Promise.resolve(0));
  %OptimizeFunctionOnNextCall(f);
  const ret = f(Promise.resolve(99));
  assertInstanceof(ret, Promise);
  assertPromiseResult(ret, r => {
    assertEquals(99, r);
    assertOptimized(f);
  });
})();

(function MixedFastAndSlow() {
  async function f(v) {
    return await v;
  }
  %PrepareFunctionForOptimization(f);
  assertPromiseResult(f(Promise.resolve(1)), r => assertEquals(1, r));
  assertPromiseResult(f(7), r => assertEquals(7, r));
  assertPromiseResult(f({then(res) { res('thn'); }}),
                      r => assertEquals('thn', r));
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(Promise.resolve('a')), r => assertEquals('a', r));
  assertPromiseResult(f(42), r => assertEquals(42, r));
  assertPromiseResult(f({then(res) { res('b'); }}), r => {
    assertEquals('b', r);
    assertOptimized(f);
  });
})();
