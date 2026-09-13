// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax --turbofan


// 1. Operation Type Bailouts
(function TestNonLoadHasTrap() {
  const target = { a: 1 };
  let calls = 0;
  const handler = {
    has(t, p) {
      calls++;
      return p in t;
    }
  };
  const proxy = new Proxy(target, handler);

  function check(obj) {
    return 'a' in obj;
  }

  %PrepareFunctionForOptimization(check);
  assertTrue(check(proxy));
  assertTrue(check(proxy));
  assertEquals(2, calls);

  %OptimizeFunctionOnNextCall(check);
  assertTrue(check(proxy));
  assertEquals(3, calls);
})();

(function TestElementAccess() {
  const target = [42];
  let calls = 0;
  const handler = {
    get(t, p, r) {
      calls++;
      return t[p];
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj[0];
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(42, load(proxy));
  assertEquals(42, load(proxy));
  assertEquals(2, calls);

  %OptimizeFunctionOnNextCall(load);
  assertEquals(42, load(proxy));
  assertEquals(3, calls);
})();

(function TestStoreBailout() {
  const target = { a: 1 };
  let calls = 0;
  const handler = {
    set(t, p, v, r) {
      calls++;
      t[p] = v + 10;
      return true;
    }
  };
  const proxy = new Proxy(target, handler);

  function store(obj, val) {
    obj.a = val;
  }

  %PrepareFunctionForOptimization(store);
  store(proxy, 5);
  assertEquals(15, target.a);
  store(proxy, 6);
  assertEquals(16, target.a);
  assertEquals(2, calls);

  %OptimizeFunctionOnNextCall(store);
  store(proxy, 7);
  assertEquals(17, target.a);
  assertEquals(3, calls);
})();

// 2. Dictionary Mode Bailouts
(function TestDictionaryTarget() {
  const target = { a: 1 };
  // Force target into dictionary mode.
  for (let i = 0; i < 2000; i++) {
    target['p' + i] = i;
  }
  let calls = 0;
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

  %PrepareFunctionForOptimization(load);
  assertEquals(1, load(proxy));
  assertEquals(1, load(proxy));
  assertEquals(2, calls);

  %OptimizeFunctionOnNextCall(load);
  assertEquals(1, load(proxy));
  assertEquals(3, calls);
})();

(function TestDictionaryHandler() {
  const target = { a: 1 };
  const handler = {
    get(t, p, r) {
      return t[p] + 1;
    }
  };
  // Force handler into dictionary mode.
  for (let i = 0; i < 2000; i++) {
    handler['p' + i] = i;
  }
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(2, load(proxy));
  assertEquals(2, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(2, load(proxy));
})();

// 3. Handler & Trap Config Bailouts
(function TestInheritedGetTrap() {
  const target = { a: 42 };
  const proto_handler = {
    get(t, p, r) {
      return t[p] + 1;
    }
  };
  const handler = Object.create(proto_handler);
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(43, load(proxy));
  assertEquals(43, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(43, load(proxy));
})();

(function TestAccessorGetTrap() {
  const target = { a: 42 };
  const handler = {
    get get() {
      return function(t, p, r) {
        return t[p] + 2;
      };
    }
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(44, load(proxy));
  assertEquals(44, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(44, load(proxy));
})();

(function TestClassConstructorGetTrap() {
  const target = { a: 42 };
  class TrapClass {
    constructor() {
      return 100;
    }
  }
  const handler = {
    get: TrapClass
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    try {
      return obj.a;
    } catch(e) {
      return "thrown";
    }
  }

  %PrepareFunctionForOptimization(load);
  assertEquals("thrown", load(proxy));
  assertEquals("thrown", load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals("thrown", load(proxy));
})();

(function TestBoundFunctionGetTrap() {
  const target = { a: 42 };
  function trap(t, p, r) {
    return t[p] + 5;
  }
  const handler = {
    get: trap.bind(null)
  };
  const proxy = new Proxy(target, handler);

  function load(obj) {
    return obj.a;
  }

  %PrepareFunctionForOptimization(load);
  assertEquals(47, load(proxy));
  assertEquals(47, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(47, load(proxy));
})();

// 4. Compiler Deoptimization Cases
(function TestTargetMapChangeDeopt() {
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
  // Mutate target map
  target.new_prop = 100;
  assertEquals(43, load(proxy));
})();

(function TestHandlerMapChangeDeopt() {
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
  // Mutate handler map
  handler.new_prop = 100;
  assertEquals(43, load(proxy));
})();

(function TestTrapMethodChangeDeopt() {
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
  // Modify the trap function on the handler (keeps map same, but changes trap target value)
  handler.get = function(t, p, r) {
    return t[p] + 10;
  };
  %PrepareFunctionForOptimization(handler.get);
  assertEquals(52, load(proxy));
})();

// 5. Positive Inlining Test
(function TestTrapInlining() {
  const target = { a: 42 };
  const handler = {
    get(t, p, r) {
      %DeoptimizeNow();
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
  assertEquals(43, load(proxy));
  assertEquals(43, load(proxy));

  %OptimizeFunctionOnNextCall(load);
  assertEquals(43, load(proxy));

  // If the trap was successfully inlined into `load`, the `%DeoptimizeNow()`
  // call inside the trap will deoptimize the `load` function itself.
  assertUnoptimized(load);
})();
