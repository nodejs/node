// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax --no-turbofan

let uniqueCounter = 0;
function makeTarget(stableMap) {
  if (!stableMap) return {};
  const obj = {};
  obj['uniqueProp_' + (uniqueCounter++)] = 1;
  return obj;
}

// 1. Test IC / CSA fast path when get trap mutates target and returns
// conflicting value.
for (const stableMap of [false, true]) {
  const target = makeTarget(stableMap);
  let shouldLock = false;
  let trapCalls = 0;
  const handler = {
    get(t, prop, receiver) {
      trapCalls++;
      if (shouldLock) {
        Object.defineProperty(t, prop, {
          value: 42,
          configurable: false,
          writable: false,
        });
        return 999;
      }
      return 42;
    }
  };
  const proxy = new Proxy(target, handler);
  function read(p) {
    return p.x;
  }
  %NeverOptimizeFunction(read);

  for (let i = 0; i < 20; i++) {
    assertEquals(42, read(proxy));
  }
  assertEquals(20, trapCalls);

  shouldLock = true;
  assertThrows(() => read(proxy), TypeError);
  assertEquals(21, trapCalls);
}

// 2. Test IC / CSA fast path when get trap mutates target and returns
// matching value.
for (const stableMap of [false, true]) {
  const target = makeTarget(stableMap);
  let shouldLock = false;
  let trapCalls = 0;
  const handler = {
    get(t, prop, receiver) {
      trapCalls++;
      if (shouldLock) {
        Object.defineProperty(t, prop, {
          value: 42,
          configurable: false,
          writable: false,
        });
        return 42;
      }
      return 42;
    }
  };
  const proxy = new Proxy(target, handler);
  function read(p) {
    return p.x;
  }
  %NeverOptimizeFunction(read);

  for (let i = 0; i < 20; i++) {
    assertEquals(42, read(proxy));
  }
  assertEquals(20, trapCalls);

  shouldLock = true;
  assertEquals(42, read(proxy));
  assertEquals(21, trapCalls);
}

// 3. Test Maglev optimized code with inlined and non-inlined
// traps, both conflicting and matching values, stable and unstable target maps,
// and try/catch exception wiring.
function runOptimizedTest(inlineTrap, returnMatching, useTryCatch, stableMap) {
  const target = makeTarget(stableMap);
  let shouldLock = false;
  let trapCalls = 0;
  function lockTarget(t, prop) {
    Object.defineProperty(t, prop, {
      value: 42,
      configurable: false,
      writable: false,
    });
    return returnMatching ? 42 : 999;
  }
  %NeverOptimizeFunction(lockTarget);
  function getTrap(t, prop, receiver) {
    trapCalls++;
    if (shouldLock) {
      return lockTarget(t, prop);
    }
    return 42;
  }
  if (!inlineTrap) {
    %NeverOptimizeFunction(getTrap);
  }
  const handler = { get: getTrap };
  const proxy = new Proxy(target, handler);

  let read;
  if (useTryCatch) {
    read = function(p) {
      try {
        return p.x;
      } catch (e) {
        return e;
      }
    };
  } else {
    read = function(p) {
      return p.x;
    };
  }

  %PrepareFunctionForOptimization(read);
  if (inlineTrap) {
    %PrepareFunctionForOptimization(getTrap);
  }
  for (let i = 0; i < 20; i++) {
    assertEquals(42, read(proxy));
  }
  %OptimizeFunctionOnNextCall(read);
  assertEquals(42, read(proxy));
  const callsBefore = trapCalls;

  shouldLock = true;
  if (returnMatching) {
    assertEquals(42, read(proxy));
  } else if (useTryCatch) {
    assertInstanceof(read(proxy), TypeError);
  } else {
    assertThrows(() => read(proxy), TypeError);
  }
  assertEquals(callsBefore + 1, trapCalls);
}

for (const inlineTrap of [true, false]) {
  for (const returnMatching of [true, false]) {
    for (const useTryCatch of [true, false]) {
      for (const stableMap of [false, true]) {
        runOptimizedTest(inlineTrap, returnMatching, useTryCatch, stableMap);
      }
    }
  }
}

// 4. Test Maglev optimized code when an eagerly inlined get trap mutates the
// target and then accesses a property on the target, giving the target a fresh
// NodeInfo whose possible_maps includes the mutated map rather than only
// expected_target_map.
{
  let shouldLock = false;
  function mutate(t) {
    if (shouldLock) {
      Object.defineProperty(t, 'x', {
        value: 42,
        configurable: false,
        writable: false,
      });
      return 999;
    }
    return 42;
  }
  %NeverOptimizeFunction(mutate);

  function getTrapWithLoad(t) {
    const r = mutate(t);
    t.y;
    return r;
  }

  %PrepareFunctionForOptimization(getTrapWithLoad);
  shouldLock = true;
  for (let i = 0; i < 10; i++) {
    const dummy = { y: 1 };
    assertEquals(999, getTrapWithLoad(dummy));
  }
  shouldLock = false;

  const target = { y: 1 };
  const proxy = new Proxy(target, { get: getTrapWithLoad });
  function readInlinedWithLoad(p) {
    return p.x;
  }
  %PrepareFunctionForOptimization(readInlinedWithLoad);
  for (let i = 0; i < 20; i++) {
    assertEquals(42, readInlinedWithLoad(proxy));
  }
  %OptimizeFunctionOnNextCall(readInlinedWithLoad);
  assertEquals(42, readInlinedWithLoad(proxy));

  shouldLock = true;
  assertThrows(() => readInlinedWithLoad(proxy), TypeError);
}

// 5. Test lazy deoptimization during the trap call (via explicit
// %DeoptimizeFunction, pre-existing stable map dependency in the caller, or
// eager deopt inside an inlined trap after mutating the target), verifying
// that ProxyGetPropertyTrapResultLazyDeoptContinuation validates the trap
// result without re-executing the trap.
for (const inlineTrap of [true, false]) {
  for (const stableMap of [false, true]) {
    const target = makeTarget(stableMap);
    let shouldLock = false;
    let trapCalls = 0;
    let read;

    function lockAndDeopt(t, prop) {
      Object.defineProperty(t, prop, {
        value: 42,
        configurable: false,
        writable: false,
      });
      %DeoptimizeFunction(read);
      return 999;
    }
    %NeverOptimizeFunction(lockAndDeopt);

    function getTrap(t, prop) {
      trapCalls++;
      if (shouldLock) {
        return lockAndDeopt(t, prop);
      }
      return 42;
    }
    if (!inlineTrap) {
      %NeverOptimizeFunction(getTrap);
    }

    const proxy = new Proxy(target, { get: getTrap });
    read = function(p, t) {
      // Touch `t` before the proxy access so a stable map dependency may
      // already be installed in the caller before TryBuildProxyPropertyAccess.
      const before = t.nonExistent;
      return p.x ?? before;
    };

    %PrepareFunctionForOptimization(read);
    if (inlineTrap) {
      %PrepareFunctionForOptimization(getTrap);
    }
    for (let i = 0; i < 20; i++) {
      assertEquals(42, read(proxy, target));
    }
    %OptimizeFunctionOnNextCall(read);
    assertEquals(42, read(proxy, target));
    const callsBefore = trapCalls;

    shouldLock = true;
    assertThrows(() => read(proxy, target), TypeError);
    assertEquals(callsBefore + 1, trapCalls);
  }
}

{
  let shouldLock = false;
  let trapCalls = 0;
  let addend = 0;

  function mutateForEagerDeopt(t) {
    trapCalls++;
    if (shouldLock) {
      Object.defineProperty(t, 'x', {
        value: 42,
        configurable: false,
        writable: false,
      });
      return 999;
    }
    return 42;
  }
  %NeverOptimizeFunction(mutateForEagerDeopt);

  function getTrapEagerDeopt(t) {
    const r = mutateForEagerDeopt(t);
    return r + addend;
  }

  const target = makeTarget(false);
  const proxy = new Proxy(target, { get: getTrapEagerDeopt });
  function readWithInlinedEagerDeopt(p) {
    return p.x;
  }

  %PrepareFunctionForOptimization(getTrapEagerDeopt);
  %PrepareFunctionForOptimization(readWithInlinedEagerDeopt);
  for (let i = 0; i < 20; i++) {
    assertEquals(42, readWithInlinedEagerDeopt(proxy));
  }
  %OptimizeFunctionOnNextCall(readWithInlinedEagerDeopt);
  assertEquals(42, readWithInlinedEagerDeopt(proxy));
  const callsBefore = trapCalls;

  shouldLock = true;
  // Trigger an eager Smi deopt on `r + addend` inside the inlined trap after
  // `mutateForEagerDeopt(t)` has already mutated `target`.
  addend = { valueOf() { return 0; } };
  assertThrows(() => readWithInlinedEagerDeopt(proxy), TypeError);
  assertEquals(callsBefore + 1, trapCalls);
}
