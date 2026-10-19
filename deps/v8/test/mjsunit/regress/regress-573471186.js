// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fast-proxy-ic

const handler = {
  get(target, property, receiver) {
    return Reflect.get(target, property, receiver);
  }
};

const proxy1 = new Proxy({a: 1}, handler);
const proxy2 = new Proxy({a: 1, b: 2}, handler);
const proxySlow = new Proxy({a: 1}, {});

function getICState(fn) {
  const feedback = %GetFeedback(fn);
  if (feedback === undefined) return undefined;
  return feedback[0][1].split('\n')[0];
}

// FastProxy -> FastProxy with different target map downgrades to slow LoadProxy
// and stays MONOMORPHIC.
(function TestFastProxyToFastProxyDowngrade() {
  function load(o) {
    return o.a;
  }
  %PrepareFunctionForOptimization(load);

  assertEquals(1, load(proxy1));
  if (getICState(load) !== undefined) {
    assertEquals('MONOMORPHIC', getICState(load));
  }

  assertEquals(1, load(proxy2));
  if (getICState(load) !== undefined) {
    assertEquals('MONOMORPHIC', getICState(load));
  }
})();

// FastProxy -> slow LoadProxy (proxy not eligible for fast path) downgrades to
// slow LoadProxy and stays MONOMORPHIC.
(function TestFastProxyToSlowProxyDowngrade() {
  function load(o) {
    return o.a;
  }
  %PrepareFunctionForOptimization(load);

  assertEquals(1, load(proxy1));
  if (getICState(load) !== undefined) {
    assertEquals('MONOMORPHIC', getICState(load));
  }

  assertEquals(1, load(proxySlow));
  if (getICState(load) !== undefined) {
    assertEquals('MONOMORPHIC', getICState(load));
  }
})();

// Polymorphic IC containing a FastProxy handler downgrades the proxy entry in
// place and stays POLYMORPHIC.
(function TestPolymorphicFastProxyDowngrade() {
  function load(o) {
    return o.a;
  }
  %PrepareFunctionForOptimization(load);

  const normalObj = {a: 42};
  assertEquals(42, load(normalObj));
  assertEquals(1, load(proxy1));
  if (getICState(load) !== undefined) {
    assertEquals('POLYMORPHIC', getICState(load));
  }

  assertEquals(1, load(proxy2));
  if (getICState(load) !== undefined) {
    assertEquals('POLYMORPHIC', getICState(load));
  }
})();
