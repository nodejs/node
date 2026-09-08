// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax

(function TestFastProxyGetWarmup() {
  let calls = 0;
  const target = { a: 1 };
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p] + 10;
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  // Uninitialized -> Miss -> ComputeHandler installs Fast Proxy LoadIC
  assertEquals(11, load(proxy));
  assertEquals(1, calls);

  // Subsequent accesses hit the fast path
  for (let i = 0; i < 10; i++) {
    assertEquals(11, load(proxy));
  }
  assertEquals(11, calls);
})();

(function TestTargetMapChangeMiss() {
  let calls = 0;
  const target = { a: 1 };
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p] + 10;
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  assertEquals(11, load(proxy));
  assertEquals(11, load(proxy));
  assertEquals(2, calls);

  // Mutate target map
  target.b = 42;
  assertEquals(11, load(proxy));
  assertEquals(3, calls);
})();

(function TestHandlerMapChangeMiss() {
  let calls = 0;
  const target = { a: 1 };
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p] + 10;
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  assertEquals(11, load(proxy));
  assertEquals(11, load(proxy));

  // Mutate handler map
  handler.extra = "prop";
  assertEquals(11, load(proxy));
  assertEquals(3, calls);
})();

(function TestTrapMethodChangeMiss() {
  let callsA = 0;
  let callsB = 0;
  const target = { a: 1 };
  const handler = {
    get(t, p, r) {
      callsA++;
      return 100;
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  assertEquals(100, load(proxy));
  assertEquals(100, load(proxy));
  assertEquals(2, callsA);

  // Mutate trap method without changing map (in-place field mutation)
  handler.get = function(t, p, r) {
    callsB++;
    return 200;
  };
  assertEquals(200, load(proxy));
  assertEquals(2, callsA);
  assertEquals(1, callsB);
})();

(function TestNonExtensibleTarget() {
  let calls = 0;
  const target = Object.preventExtensions({ a: 1 });
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p];
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  // Should fall back to slow proxy handler
  assertEquals(1, load(proxy));
  assertEquals(1, load(proxy));
  assertEquals(2, calls);
})();

(function TestNonConfigurableProperty() {
  let calls = 0;
  const target = {};
  Object.defineProperty(target, 'a', {
    value: 42,
    configurable: false,
    writable: true,
    enumerable: true
  });
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p];
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  // Should fall back to slow proxy handler due to non-configurable property
  assertEquals(42, load(proxy));
  assertEquals(42, load(proxy));
  assertEquals(2, calls);
})();

(function TestCompilerOptimizationAndReceiverMapChange() {
  const target = { a: 42 };
  const handler = {
    get(t, p, r) {
      return t[p] + 1;
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  %PrepareFunctionForOptimization(load);
  %PrepareFunctionForOptimization(handler.get);
  assertEquals(43, load(proxy));
  assertEquals(43, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(43, load(proxy));

  // Now, pass a plain object. It should deoptimize due to receiver map mismatch
  const plain = { a: 100 };
  assertEquals(100, load(plain)); // Plain load (trap is not called, returns 100)
})();
