// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Promise.prototype.then on a JSPromise with the initial Promise.prototype and
// callable-or-undefined reactions reduces to an inlined result-promise
// allocation plus a direct PerformPromiseThen call; chained values, rejection
// paths, non-callable reactions, transitioned maps and subclassed receivers
// must keep their semantics.

async function chain(p) {
  return p.then(v => v + 1).then(v => v * 2, undefined).then(42).then(
      v => v, e => -e);
}
%PrepareFunctionForOptimization(chain);

(async () => {
  assertEquals(8, await chain(Promise.resolve(3)));
  assertEquals(8, await chain(Promise.resolve(3)));
  %OptimizeFunctionOnNextCall(chain);
  assertEquals(8, await chain(Promise.resolve(3)));
  assertEquals(12, await chain(Promise.resolve(5)));

  // Rejection flows through the untouched second reaction.
  async function rejecting(p) {
    return p.then(v => v + 1, e => 1000 + e);
  }
  %PrepareFunctionForOptimization(rejecting);
  assertEquals(1007, await rejecting(Promise.reject(7)));
  %OptimizeFunctionOnNextCall(rejecting);
  assertEquals(1007, await rejecting(Promise.reject(7)));
  assertEquals(4, await rejecting(Promise.resolve(3)));

  // Subclassed promise receiver deopts safely and stays correct.
  class MyPromise extends Promise {}
  async function sub(p) {
    return p.then(v => v + 1);
  }
  %PrepareFunctionForOptimization(sub);
  assertEquals(2, await sub(Promise.resolve(1)));
  %OptimizeFunctionOnNextCall(sub);
  assertEquals(2, await sub(Promise.resolve(1)));
  assertEquals(3, await sub(MyPromise.resolve(2)));

  // A promise whose map transitioned away from the initial map (added own
  // property) but keeps the initial Promise.prototype still reduces.
  async function expando(p) {
    return p.then(v => v + 1);
  }
  %PrepareFunctionForOptimization(expando);
  function taggedPromise(v) {
    let p = Promise.resolve(v);
    p.extra = 1;
    return p;
  }
  assertEquals(11, await expando(taggedPromise(10)));
  %OptimizeFunctionOnNextCall(expando);
  assertEquals(21, await expando(taggedPromise(20)));
})();
