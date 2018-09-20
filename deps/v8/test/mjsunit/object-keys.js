// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Ensure that mutation of the Object.keys result doesn't affect the
// enumeration cache for fast-mode objects.
(function() {
  const a = {x:1, y:2};
  let k = Object.keys(a);
  %HeapObjectVerify(k);
  assertEquals(2, k.length);
  assertEquals("x", k[0]);
  assertEquals("y", k[1]);
  k[0] = "y";
  k[1] = "x";
  k = Object.keys(a);
  assertEquals(2, k.length);
  assertEquals("x", k[0]);
  assertEquals("y", k[1]);
})();

// Ensure that the copy-on-write keys are handled properly, even in
// the presence of Symbols.
(function() {
  const s = Symbol();
  const a = {[s]: 1};
  let k = Object.keys(a);
  %HeapObjectVerify(k);
  assertEquals(0, k.length);
  k.shift();
  assertEquals(0, k.length);
})();

// Ensure we invoke all steps on proxies.
(function ObjectKeysProxy() {
  let log = [];
  let result = Object.keys(new Proxy({}, {
    ownKeys(target) {
      log.push('ownKeys');
      return ['a', 'b', 'c'];
    },
    getOwnPropertyDescriptor(target, key) {
      log.push('getOwnPropertyDescriptor-' + key);
      if (key === 'b') return {enumerable: false, configurable: true};
      return {enumerable: true, configurable: true};
    }
  }));
  assertEquals(['a', 'c'], result);
  assertEquals(
      [
        'ownKeys', 'getOwnPropertyDescriptor-a', 'getOwnPropertyDescriptor-b',
        'getOwnPropertyDescriptor-c'
      ],
      log);

  // Test normal target.
  log = [];
  let target = {a: 1, b: 1, c: 1};
  let handler = {
    getOwnPropertyDescriptor(target, key) {
      log.push('getOwnPropertyDescriptor-' + key);
      if (key === 'b') return {enumerable: false, configurable: true};
      return {enumerable: true, configurable: true};
    }
  };
  result = Object.keys(new Proxy(target, handler));
  assertEquals(['a', 'c'], result);
  assertEquals(
      [
        'getOwnPropertyDescriptor-a', 'getOwnPropertyDescriptor-b',
        'getOwnPropertyDescriptor-c'
      ],
      log);

  // Test trap invocation with non-enumerable target properties.
  log = [];
  target = Object.create(Object.prototype, {
    a: {enumerable: true, configurable: true},
    b: {enumerable: false, configurable: true},
    c: {enumerable: true, configurable: true}
  });
  result = Object.keys(new Proxy(target, handler));
  assertEquals(['a', 'c'], result);
  assertEquals(
      [
        'getOwnPropertyDescriptor-a', 'getOwnPropertyDescriptor-b',
        'getOwnPropertyDescriptor-c'
      ],
      log);
})();
