// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function createDictObject() {
  const o = {a: 1, b: 2, c: 3};
  delete o.b;
  assertFalse(%HasFastProperties(o));
  return o;
}

if (%IsDictPropertyConstTrackingEnabled()) {
  // 1. Normalization preserves fast property constness and sets new dictionary
  // properties to kConst.
  (function testNormalizationAndRuntimeStore() {
    const o = {fastConst: 10, fastMutable: 20};
    o.fastMutable = 21;
    assertTrue(%HasOwnConstDataProperty(o, 'fastConst'));
    assertFalse(%HasOwnConstDataProperty(o, 'fastMutable'));

    // Normalize to dictionary mode via delete.
    o.toDelete = 1;
    delete o.toDelete;
    assertFalse(%HasFastProperties(o));

    assertTrue(%HasOwnConstDataProperty(o, 'fastConst'));
    assertFalse(%HasOwnConstDataProperty(o, 'fastMutable'));

    // Adding a new property in dictionary mode initializes it as kConst.
    o.fresh = 42;
    assertTrue(%HasOwnConstDataProperty(o, 'fresh'));

    // Storing the same value keeps it kConst.
    o.fresh = 42;
    assertTrue(%HasOwnConstDataProperty(o, 'fresh'));

    // Storing a different value via runtime/uninitialized IC marks it kMutable.
    o.fresh = 43;
    assertFalse(%HasOwnConstDataProperty(o, 'fresh'));
  })();

  // 2. Named StoreIC handler (StoreNormal) preserves kConst on same-value
  // stores and invalidates to kMutable in-place on different-value stores.
  (function testNamedStoreIC() {
    function storeX(obj, val) {
      obj.x = val;
    }

    // Warm up StoreIC on dictionary-mode receivers.
    for (let i = 0; i < 10; i++) {
      const warm = createDictObject();
      warm.x = 100;
      storeX(warm, 100);
    }

    const o = createDictObject();
    o.x = 100;
    assertTrue(%HasOwnConstDataProperty(o, 'x'));

    // Same-value store through warm StoreNormal IC keeps kConst.
    storeX(o, 100);
    assertTrue(%HasOwnConstDataProperty(o, 'x'));

    // Different-value store through warm StoreNormal IC flips to kMutable.
    storeX(o, 200);
    assertEquals(200, o.x);
    assertFalse(%HasOwnConstDataProperty(o, 'x'));

    // Subsequent store of same value does not make it kConst again.
    storeX(o, 200);
    assertFalse(%HasOwnConstDataProperty(o, 'x'));
  })();

  // 3. Store through prototype chain IC handler (HandleStoreICProtoHandler)
  // when receiver is in dictionary mode and property already exists on
  // receiver.
  (function testProtoStoreICOnDictReceiver() {
    const proto = {};
    Object.defineProperty(proto, 'p', {
      value: 1,
      writable: true,
      enumerable: true,
      configurable: true,
    });

    function storeP(obj, val) {
      obj.p = val;
    }

    for (let i = 0; i < 10; i++) {
      const warm = Object.create(proto);
      warm.a = 1;
      warm.b = 2;
      delete warm.a;
      assertFalse(%HasFastProperties(warm));
      storeP(warm, 10);
    }

    const o = Object.create(proto);
    o.a = 1;
    o.b = 2;
    delete o.a;
    assertFalse(%HasFastProperties(o));

    // Adding 'p' on dictionary receiver initializes it as kConst.
    storeP(o, 55);
    assertTrue(%HasOwnConstDataProperty(o, 'p'));

    // Same value keeps kConst.
    storeP(o, 55);
    assertTrue(%HasOwnConstDataProperty(o, 'p'));

    // Different value flips to kMutable.
    storeP(o, 66);
    assertEquals(66, o.p);
    assertFalse(%HasOwnConstDataProperty(o, 'p'));
  })();

  // 4. KeyedStoreIC / KeyedStoreGeneric preserves kConst on same-value stores
  // and invalidates to kMutable in-place on different-value stores.
  (function testKeyedStoreIC() {
    function keyedStore(obj, key, val) {
      obj[key] = val;
    }

    for (let i = 0; i < 15; i++) {
      const warm = createDictObject();
      const k = 'k' + i;
      warm[k] = 1;
      keyedStore(warm, k, 1);
    }

    const o = createDictObject();
    keyedStore(o, 'prop', 777);
    assertTrue(%HasOwnConstDataProperty(o, 'prop'));

    // Same value via KeyedStoreGeneric keeps kConst.
    keyedStore(o, 'prop', 777);
    assertTrue(%HasOwnConstDataProperty(o, 'prop'));

    // Different value via KeyedStoreGeneric flips to kMutable.
    keyedStore(o, 'prop', 888);
    assertEquals(888, o.prop);
    assertFalse(%HasOwnConstDataProperty(o, 'prop'));
  })();
} else {
  const o = createDictObject();
  assertFalse(%HasOwnConstDataProperty(o, 'a'));
  assertFalse(%HasOwnConstDataProperty(o, 'c'));

  o.fresh = 42;
  assertFalse(%HasOwnConstDataProperty(o, 'fresh'));
}
