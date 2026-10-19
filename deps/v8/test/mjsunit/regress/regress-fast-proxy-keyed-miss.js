// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fast-proxy-ic

const proxy = new Proxy({ a: 1 }, {
  get(target, prop, receiver) {
    return prop;
  }
});

const proxy2 = new Proxy({ a: 1, b: 2 }, {
  get(target, prop, receiver) {
    return prop;
  }
});

function getICState(fn) {
  const feedback = %GetFeedback(fn);
  if (feedback === undefined) return undefined;
  return feedback[0][1].split("\n")[0];
}

// Test keyed loads with primitive keys (undefined, null, true, false, 1.5).
function testPrimitiveKey(key, expected) {
  function load(p, k) {
    return p[k];
  }
  %PrepareFunctionForOptimization(load);

  for (let i = 0; i < 1000; i++) {
    assertEquals(expected, load(proxy, key));
  }
}

testPrimitiveKey(undefined, "undefined");
testPrimitiveKey(null, "null");
testPrimitiveKey(true, "true");
testPrimitiveKey(false, "false");
testPrimitiveKey(1.5, "1.5");

// On 64-bit platforms, non-internalized integer index strings > kMaxArrayIndex
// (2^32 - 2) bail out of CSA TryInternalizeString to Runtime_KeyedLoadIC_Miss,
// where TryConvertKey internalizes them as kName. Verify that the proxy IC
// progresses to the same state (MEGAMORPHIC on 64-bit) as a normal object IC
// instead of looping in MONOMORPHIC and allocating a new DataHandler on every
// call.
(function TestUncacheableIntegerIndexStringKeyedMiss() {
  const prefix = "500000";
  const normalObj = { a: 1, 5000000000: "5000000000" };

  function loadNormal(o, k) {
    return o[k];
  }
  %PrepareFunctionForOptimization(loadNormal);
  assertEquals("5000000000", loadNormal(normalObj, prefix + "0000"));
  assertEquals("5000000000", loadNormal(normalObj, prefix + "0000"));
  const expectedState = getICState(loadNormal);

  function load(p, k) {
    return p[k];
  }
  %PrepareFunctionForOptimization(load);

  assertEquals("5000000000", load(proxy, prefix + "0000"));
  if (expectedState !== undefined) {
    assertEquals("MONOMORPHIC", getICState(load));
  }

  assertEquals("5000000000", load(proxy, prefix + "0000"));
  if (expectedState !== undefined) {
    assertEquals(expectedState, getICState(load));
  }

  for (let i = 0; i < 1000; i++) {
    assertEquals("5000000000", load(proxy, prefix + "0000"));
  }
})();

// Test when the slot was already downgraded from LoadProxyFast to LoadProxy
// Smi handler before missing again with a key that converts to the same Name.
(function TestDowngradedProxyIntegerIndexKeyedMiss() {
  const prefix = "500000";
  const normalObj = { a: 1, 5000000000: "5000000000" };

  function loadNormal(o, k) {
    return o[k];
  }
  %PrepareFunctionForOptimization(loadNormal);
  loadNormal(normalObj, prefix + "0000");
  loadNormal(normalObj, prefix + "0000");
  const expectedState = getICState(loadNormal);

  function load(p, k) {
    return p[k];
  }
  %PrepareFunctionForOptimization(load);

  // Install LoadProxyFast for "5000000000" (on 32-bit) or LoadProxy Smi
  // (on 64-bit, where "5000000000" is an integer index).
  assertEquals("5000000000", load(proxy, "5000000000"));
  // Downgrade to slow LoadProxy Smi handler (proxy2 has a different target map).
  assertEquals("5000000000", load(proxy2, "5000000000"));

  if (expectedState !== undefined) {
    assertEquals("MONOMORPHIC", getICState(load));
  }

  // Miss with non-internalized integer index string "500000" + "0000", which
  // converts to "5000000000" with state() != RECOMPUTE_HANDLER. Since the
  // handler is already slow LoadProxy, it must transition to MEGAMORPHIC on
  // 64-bit platforms.
  assertEquals("5000000000", load(proxy2, prefix + "0000"));

  if (expectedState !== undefined) {
    assertEquals(expectedState, getICState(load));
  }

  for (let i = 0; i < 1000; i++) {
    assertEquals("5000000000", load(proxy2, prefix + "0000"));
  }
})();

(function TestDowngradedProxyKeyedMiss() {
  const normalObj = { a: 1, "1.5": "1.5" };

  function loadNormal(o, k) {
    return o[k];
  }
  %PrepareFunctionForOptimization(loadNormal);
  loadNormal(normalObj, "1.5");
  loadNormal(normalObj, 1.5);
  const expectedState = getICState(loadNormal);

  function load(p, k) {
    return p[k];
  }
  %PrepareFunctionForOptimization(load);

  // Install LoadProxyFast for "1.5".
  assertEquals("1.5", load(proxy, "1.5"));
  // Downgrade to slow LoadProxy Smi handler (proxy2 has a different target map).
  assertEquals("1.5", load(proxy2, "1.5"));

  if (expectedState !== undefined) {
    assertEquals("MONOMORPHIC", getICState(load));
  }

  // Miss with HeapNumber key 1.5, which converts to "1.5" in TryConvertKey
  // with state() != RECOMPUTE_HANDLER. Since the handler is already slow
  // LoadProxy, it must transition to MEGAMORPHIC.
  assertEquals("1.5", load(proxy2, 1.5));

  if (expectedState !== undefined) {
    assertEquals(expectedState, getICState(load));
  }

  for (let i = 0; i < 1000; i++) {
    assertEquals("1.5", load(proxy2, 1.5));
  }
})();

// Test monomorphic -> polymorphic IC transition with fast proxy handler.
(function TestFastProxyPolymorphicTransition() {
  function load(obj) {
    return obj.a;
  }
  %PrepareFunctionForOptimization(load);

  for (let i = 0; i < 100; i++) {
    assertEquals("a", load(proxy));
  }

  if (getICState(load) !== undefined) {
    assertEquals("MONOMORPHIC", getICState(load));
  }

  const normalObj = { a: 42 };
  // Load twice: with --sparkplug-plus, the first execution of the load in
  // (concurrently installed) baseline code may only patch the IC call target
  // without updating the feedback.
  assertEquals(42, load(normalObj));
  assertEquals(42, load(normalObj));

  if (getICState(load) !== undefined) {
    assertEquals("POLYMORPHIC", getICState(load));
  }
})();
