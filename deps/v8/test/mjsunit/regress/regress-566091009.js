// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --allow-natives-syntax

(function TestAccessCheck() {
  const target = d8.test.createAccessCheckedObject(true);
  target[0] = "secret";
  d8.test.setAccessPolicy(target, false);

  const namedObj = { a: 1 };

  // 1. KeyedLoadIC: transition from named property access to element access.
  function read(obj, key) {
    return obj[key];
  }
  read(namedObj, "a");
  assertThrows(() => read(target, "a"), TypeError);
  read([1.1], 0);
  assertThrows(() => read(target, 0), TypeError);

  // 2. KeyedHasIC: transition from named property access to element access.
  function has(obj, key) {
    return key in obj;
  }
  has(namedObj, "a");
  assertThrows(() => has(target, "a"), TypeError);
  has([1.1], 0);
  assertThrows(() => has(target, 0), TypeError);

  // 3. KeyedStoreIC: transition from named property access to element access.
  function write(obj, key, val) {
    obj[key] = val;
  }
  write(namedObj, "a", 2);
  assertThrows(() => write(target, "a", 2), TypeError);
  write([1.1], 0, 2.2);
  assertThrows(() => write(target, 0, "overwritten"), TypeError);

  d8.test.setAccessPolicy(target, true);
  assertEquals("secret", target[0]);
})();

(function TestNoElementsProtector() {
  function readHole(a) {
    return a[0];
  }
  const holey = [, 1];
  %PrepareFunctionForOptimization(readHole);
  assertEquals(undefined, readHole(holey));
  %OptimizeFunctionOnNextCall(readHole);
  assertEquals(undefined, readHole(holey));

  Object.defineProperty(Object.prototype, "prop", {
    set(_) {},
    configurable: true,
  });
  function write(o, k, v) {
    o[k] = v;
  }
  write({ prop: 1 }, "prop", 1);
  write(Array.prototype, "prop", 1);
  write([], 0, 1);
  write(Array.prototype, 0, "boom");

  assertEquals("boom", holey[0]);
  assertEquals("boom", readHole(holey));
})();
