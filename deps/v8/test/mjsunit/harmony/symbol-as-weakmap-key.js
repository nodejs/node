// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --noincremental-marking

(function TestWeakMapWithNonRegisteredSymbolKey() {
  const key = Symbol('123');
  const value = 1;
  const map = new WeakMap();
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
  assertFalse(map.delete(key));
  assertSame(map, map.set(key, value));
  assertSame(value, map.get(key));
  assertTrue(map.has(key));
  assertTrue(map.delete(key));
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
  assertFalse(map.delete(key));
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
})();

(function TestWeakMapWithNonRegisteredSymbolKeyGC() {
  const map = new WeakMap();

  const outerKey = Symbol('234');
  const outerValue = 1;
  map.set(outerKey, outerValue);
  (function () {
    const innerKey = Symbol('123');
    const innerValue = 1;
    map.set(innerKey, innerValue);
    assertTrue(map.has(innerKey));
    assertSame(innerValue, map.get(innerKey));
  })();

  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, some objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  (async function () {
    await gc({ type: 'major', execution: 'async' });
    assertTrue(map.has(outerKey));
    assertSame(outerValue, map.get(outerKey));
    assertEquals(1, %GetWeakCollectionSize(map));
  })();
})();

(function TestWeakMapWithRegisteredSymbolKey() {
  const key = Symbol.for('123');
  const value = 1;
  const map = new WeakMap();
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
  assertFalse(map.delete(key));
  assertThrows(() => {
    map.set(key, value);
  }, TypeError, 'Invalid value used as weak map key');
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
  assertFalse(map.delete(key));
  assertFalse(map.has(key));
  assertSame(undefined, map.get(key));
})();

(function TestWeakSetWithNonRegisteredSymbolKey() {
  const key = Symbol('123');
  const set = new WeakSet();
  assertFalse(set.has(key));
  assertFalse(set.delete(key));
  assertSame(set, set.add(key));
  assertTrue(set.has(key));
  assertTrue(set.delete(key));
  assertFalse(set.has(key));
  assertFalse(set.delete(key));
  assertFalse(set.has(key));
})();

(function TestWeakSetWithNonRegisteredSymbolKeyGC() {
  const set = new WeakSet();
  const outerKey = Symbol('234');
  set.add(outerKey);
  (function () {
    const innerKey = Symbol('123');
    set.add(innerKey);
    assertTrue(set.has(innerKey));
  })();
  assertTrue(set.has(outerKey));
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, some objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  (async function () {
    await gc({ type: 'major', execution: 'async' });
    assertTrue(set.has(outerKey));
    assertEquals(1, %GetWeakCollectionSize(set));
  })();
})();

(function TestWeakSetWithRegisteredSymbolKey() {
  const key = Symbol.for('123');
  const set = new WeakSet();
  assertFalse(set.has(key));
  assertFalse(set.delete(key));

  assertThrows(() => {
    assertSame(set, set.add(key));
  }, TypeError, 'Invalid value used in weak set');

  assertFalse(set.has(key));
  assertFalse(set.delete(key));
  assertFalse(set.has(key));
})();

(function TestFinalizationRegistryUnregister() {
  const fr = new FinalizationRegistry(function() {});
  const key = {};
  fr.register(Symbol('foo'), "holdings", key);
  fr.unregister(key);
})();
