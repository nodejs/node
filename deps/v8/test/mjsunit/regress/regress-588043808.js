// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const maxLenSymbol = Symbol('a'.repeat(%StringMaxLength()));

(function TestSyncTryCatch() {
  function f(key) {
    try {
      return { [key]: () => {} };
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(f);
  assertEquals('x', f('x').x.name);
  assertEquals('y', f('y').y.name);
  %OptimizeFunctionOnNextCall(f);
  assertInstanceof(f(maxLenSymbol), RangeError);
})();

(function TestAsyncGeneratorReject() {
  const keys = ['x', 'y', maxLenSymbol];
  let idx = 0;
  function getKey() {
    return keys[idx++];
  }
  async function* ag() {
    return { [getKey()]: () => {} };
  }

  %PrepareFunctionForOptimization(ag);
  assertPromiseResult(ag().next(), r => {
    assertTrue(r.done);
    assertEquals('x', r.value.x.name);
  });
  assertPromiseResult(ag().next(), r => {
    assertTrue(r.done);
    assertEquals('y', r.value.y.name);
  });
  %OptimizeFunctionOnNextCall(ag);
  assertPromiseResult(
      ag().next(), assertUnreachable,
      e => assertInstanceof(e, RangeError));
})();
