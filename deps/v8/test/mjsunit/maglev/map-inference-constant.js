// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Test MapInference on constant HeapObject receivers and proper stability
// dependency invalidation when the constant's map transitions.

// 1. Constant RegExp receiver with RegExp.prototype.test.
{
  const re = /foo/;
  function testRe(s) {
    return re.test(s);
  }
  %PrepareFunctionForOptimization(testRe);
  assertTrue(testRe("foo"));
  assertFalse(testRe("bar"));
  %OptimizeFunctionOnNextCall(testRe);
  assertTrue(testRe("foo"));
  assertFalse(testRe("bar"));
  assertOptimized(testRe);

  // Transitioning the constant RegExp's map by defining an own `exec` property
  // must invalidate the stability dependency and deoptimize `testRe`.
  let execCalled = false;
  re.exec = function(s) {
    execCalled = true;
    return null;
  };
  assertFalse(testRe("foo"));
  assertTrue(execCalled);
}

// 2. Constant WeakMap receiver with WeakMap.prototype.get.
{
  const wm = new WeakMap();
  const k1 = {};
  const k2 = {};
  wm.set(k1, 42);
  function getWm(k) {
    return wm.get(k);
  }
  %PrepareFunctionForOptimization(getWm);
  assertEquals(42, getWm(k1));
  assertEquals(undefined, getWm(k2));
  %OptimizeFunctionOnNextCall(getWm);
  assertEquals(42, getWm(k1));
  assertEquals(undefined, getWm(k2));
  assertOptimized(getWm);

  // Transitioning the constant WeakMap's map must keep working.
  wm.extra = 1;
  assertEquals(42, getWm(k1));
  assertEquals(undefined, getWm(k2));
}

// 3. Constant Map receiver with Map.prototype.get.
{
  const m = new Map([["a", 1], ["b", 2]]);
  function getMap(k) {
    return m.get(k);
  }
  %PrepareFunctionForOptimization(getMap);
  assertEquals(1, getMap("a"));
  assertEquals(undefined, getMap("c"));
  %OptimizeFunctionOnNextCall(getMap);
  assertEquals(1, getMap("a"));
  assertEquals(undefined, getMap("c"));
  assertOptimized(getMap);

  m.extra = 1;
  assertEquals(2, getMap("b"));
}

// 4. Constant Set receiver with Set.prototype.has.
{
  const s = new Set(["a", "b"]);
  function hasSet(k) {
    return s.has(k);
  }
  %PrepareFunctionForOptimization(hasSet);
  assertTrue(hasSet("a"));
  assertFalse(hasSet("c"));
  %OptimizeFunctionOnNextCall(hasSet);
  assertTrue(hasSet("a"));
  assertFalse(hasSet("c"));
  assertOptimized(hasSet);

  s.extra = 1;
  assertTrue(hasSet("b"));
  assertFalse(hasSet("z"));
}

// 5. Constant Array receiver with Array.prototype.push and map transition.
{
  const arr = [1, 2];
  function pushArr(x) {
    return arr.push(x);
  }
  %PrepareFunctionForOptimization(pushArr);
  assertEquals(3, pushArr(3));
  assertEquals(4, pushArr(4));
  %OptimizeFunctionOnNextCall(pushArr);
  assertEquals(5, pushArr(5));
  assertOptimized(pushArr);

  // Transition elements kind from PACKED_SMI to PACKED_ELEMENTS.
  assertEquals(6, pushArr("six"));
  assertEquals("six", arr[5]);
}

// 6. Constant receiver with Object.getPrototypeOf and prototype change.
{
  const o = {x: 1};
  const proto = {y: 2};
  function getProto() {
    return Object.getPrototypeOf(o);
  }
  %PrepareFunctionForOptimization(getProto);
  assertSame(Object.prototype, getProto());
  %OptimizeFunctionOnNextCall(getProto);
  assertSame(Object.prototype, getProto());
  assertOptimized(getProto);

  Object.setPrototypeOf(o, proto);
  assertSame(proto, getProto());
}

// 7. A value that was only checked to equal a constant number does not have
// the constant's map: the Smi 2 passes the check against the HeapNumber 2.
{
  function add(a, b) {
    return a + b;
  }
  globalThis.heapNumber = add(1.5, 0.5);
  const o = {p: {}};
  o.p = 1;
  o.p = 's';
  function storeNumber(v) {
    heapNumber = v;
    Array.isArray(v);
    o.p = v;
    return o.p;
  }
  %PrepareFunctionForOptimization(storeNumber);
  assertEquals(2, storeNumber(heapNumber));
  assertEquals(2, storeNumber(heapNumber));
  %OptimizeFunctionOnNextCall(storeNumber);
  assertEquals(2, storeNumber(heapNumber));
  assertEquals(2, storeNumber(2));
}

// 8. The known type of a constant can contradict its map: undefined is
// narrowed to a boolean by the comparison, which then always deopts.
{
  function compareAndGetProto(a, skip) {
    if (skip) return 0;
    const r = a == 1;
    return [r, Object.getPrototypeOf(a)];
  }
  function callWithUndefined(skip) {
    return compareAndGetProto(undefined, skip);
  }
  %PrepareFunctionForOptimization(compareAndGetProto);
  %PrepareFunctionForOptimization(callWithUndefined);
  compareAndGetProto(true, false);
  compareAndGetProto(false, false);
  assertEquals(0, callWithUndefined(true));
  %OptimizeFunctionOnNextCall(callWithUndefined);
  assertEquals(0, callWithUndefined(true));
  assertThrows(() => callWithUndefined(false), TypeError);
}
