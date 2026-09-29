// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
// Flags: --no-stress-opt --no-stress-incremental-marking

function check(fn, expected) {
  %PrepareFunctionForOptimization(fn);
  let r0 = fn();
  let r1 = fn();
  %OptimizeMaglevOnNextCall(fn);
  let r2 = fn();
  assertTrue(isMaglevved(fn));
  return Promise.all([r0, r1, r2]).then(([v0, v1, v2]) => {
    assertEquals(expected, v0);
    assertEquals(expected, v1);
    assertEquals(expected, v2);
  });
}

(async () => {
  async function ret_smi() { return 42; }
  async function ret_undefined() { /* implicit undefined */ }
  async function ret_null() { return null; }
  async function ret_true() { return true; }
  async function ret_false() { return false; }
  async function ret_string() { return "hello"; }
  async function ret_heap_number() { return 1.5; }
  async function ret_bigint() { return 7n; }

  await check(ret_smi, 42);
  await check(ret_undefined, undefined);
  await check(ret_null, null);
  await check(ret_true, true);
  await check(ret_false, false);
  await check(ret_string, "hello");
  await check(ret_heap_number, 1.5);
  await check(ret_bigint, 7n);

  // A .then handler attached after the function returns still observes the
  // fulfilled value. With the inlining, the promise reaches the .then call
  // already in the fulfilled state.
  async function ret_smi_2() { return 99; }
  %PrepareFunctionForOptimization(ret_smi_2);
  await ret_smi_2();
  %OptimizeMaglevOnNextCall(ret_smi_2);
  let observed;
  await ret_smi_2().then(v => { observed = v; });
  assertEquals(99, observed);
  assertTrue(isMaglevved(ret_smi_2));

  // Non-primitive (Proxy) return: must NOT take the inlined path, because
  // JSResolvePromise's "then" check is required. The Proxy below has no
  // own/inherited "then" so it eventually fulfills with the Proxy itself.
  async function ret_proxy(p) { return p; }
  %PrepareFunctionForOptimization(ret_proxy);
  const proxy = new Proxy({}, {});
  assertEquals(proxy, await ret_proxy(proxy));
  %OptimizeMaglevOnNextCall(ret_proxy);
  assertEquals(proxy, await ret_proxy(proxy));
  assertTrue(isMaglevved(ret_proxy));

  // With an actual await, the async function object escapes through
  // AsyncFunctionAwait, so the inlining must not fire. Just check correctness.
  async function ret_after_await(x) {
    await 0;
    return x;
  }
  %PrepareFunctionForOptimization(ret_after_await);
  assertEquals(123, await ret_after_await(123));
  assertEquals(123, await ret_after_await(123));
  %OptimizeMaglevOnNextCall(ret_after_await);
  assertEquals(123, await ret_after_await(123));
  assertEquals("ok", await ret_after_await("ok"));
})();
