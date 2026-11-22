// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax --maglev
// Flags: --optimize-on-next-call-optimizes-to-maglev

function makeHandler(x) {
  return {
    x,
    get(target, prop, receiver) {
      return target[prop];
    },
  };
}

// Test 1: Deprecated handler map in FastProxy DataHandler must not abort
// Maglev compilation when committing field dependencies.
(function TestDeprecatedHandlerMap() {
  let h1 = makeHandler(1);
  let h2 = makeHandler(1);
  let p = new Proxy({a: 42}, h1);

  function loadFromProxy(proxy, run) {
    if (run) return proxy.a;
    return 0;
  }

  %PrepareFunctionForOptimization(loadFromProxy);
  for (let i = 0; i < 10; i++) {
    assertEquals(42, loadFromProxy(p, true));
  }

  // Transition x from Smi to Double on h2, deprecating h1's map while h1
  // remains unmigrated and the proxy LoadIC DataHandler still holds h1's
  // deprecated map.
  h2.x = 1.5;

  loadFromProxy(p, false);
  %OptimizeFunctionOnNextCall(loadFromProxy);
  assertEquals(0, loadFromProxy(p, false));
  assertOptimized(loadFromProxy);
  assertEquals(42, loadFromProxy(p, true));
})();

// Test 2: Deprecated target map in FastProxy DataHandler must not emit a
// stale CheckMaps on the deprecated target map that immediately deopts.
(function TestDeprecatedTargetMap() {
  function makeTarget(x) {
    return {a: 42, x};
  }
  let t1 = makeTarget(1);
  let t2 = makeTarget(1);
  let h = {
    get(target, prop, receiver) {
      return target[prop];
    },
  };
  let p = new Proxy(t1, h);

  function loadFromProxyTarget(proxy) {
    return proxy.a;
  }

  %PrepareFunctionForOptimization(loadFromProxyTarget);
  for (let i = 0; i < 10; i++) {
    assertEquals(42, loadFromProxyTarget(p));
  }

  // Deprecate t1's map via t2 and migrate t1 by reading a property on t1.
  t2.x = 1.5;
  assertEquals(1, t1.x);

  // Compile before executing loadFromProxyTarget again (so the DataHandler
  // still holds t1's old deprecated map).
  %OptimizeFunctionOnNextCall(loadFromProxyTarget);
  assertEquals(42, loadFromProxyTarget(p));
  assertOptimized(loadFromProxyTarget);
})();
