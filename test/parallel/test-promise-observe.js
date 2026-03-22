'use strict';
// This test ensures that observePromise() lets APM/diagnostic tools observe
// promise outcomes without suppressing the `unhandledRejection` event.
// Flags: --expose-internals
const common = require('../common');
const assert = require('node:assert');
const { observePromise } = require('internal/promise_observe');

// Execution order within a tick: nextTick queue → microtasks → processPromiseRejections
// (which fires unhandledRejection). setImmediate runs in the next event loop
// iteration after all of the above, so we use it for final assertions.
//
// All tests share the same process, so we track unhandledRejection by promise
// identity rather than using process.once, which would fire for all rejections.

// Track all unhandled rejections by promise identity.
const unhandledByPromise = new Map();
process.on('unhandledRejection', (reason, promise) => {
  unhandledByPromise.set(promise, reason);
});

// --- Test 1: Observe a rejected promise — unhandledRejection still fires ---
{
  const err1 = new Error('test1');
  const p1 = Promise.reject(err1);

  observePromise(p1, null, common.mustCall((err) => {
    assert.strictEqual(err, err1);
  }));

  setImmediate(common.mustCall(() => {
    assert.ok(unhandledByPromise.has(p1), 'Test 1: unhandledRejection should have fired');
    assert.strictEqual(unhandledByPromise.get(p1), err1);
  }));
}

// --- Test 2: Observe then add real handler — no unhandledRejection ---
{
  const err2 = new Error('test2');
  const p2 = Promise.reject(err2);

  observePromise(p2, null, common.mustCall(() => {}));

  // Real handler added synchronously after observing.
  p2.catch(() => {});

  setImmediate(common.mustCall(() => {
    assert.ok(!unhandledByPromise.has(p2), 'Test 2: unhandledRejection should NOT have fired');
  }));
}

// --- Test 3: Observe pending promise that later rejects ---
{
  const err3 = new Error('test3');
  const { promise: p3, reject: reject3 } = Promise.withResolvers();

  observePromise(p3, null, common.mustCall((err) => {
    assert.strictEqual(err, err3);
  }));

  reject3(err3);

  // Two rounds of setImmediate to ensure both the observer callback and
  // unhandledRejection have had a chance to run.
  setImmediate(common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      assert.ok(unhandledByPromise.has(p3), 'Test 3: unhandledRejection should have fired');
    }));
  }));
}

// --- Test 4: Observe pending promise that fulfills — no warnings ---
{
  const { promise: p4, resolve: resolve4 } = Promise.withResolvers();

  observePromise(p4, common.mustCall((val) => {
    assert.strictEqual(val, 42);
  }), null);

  resolve4(42);

  setImmediate(common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      assert.ok(!unhandledByPromise.has(p4), 'Test 4: unhandledRejection should NOT have fired');
    }));
  }));
}

// --- Test 5: Multiple observers — all called, unhandledRejection still fires ---
{
  const err5 = new Error('test5');
  const p5 = Promise.reject(err5);

  observePromise(p5, null, common.mustCall(() => {}));
  observePromise(p5, null, common.mustCall(() => {}));

  setImmediate(common.mustCall(() => {
    assert.ok(unhandledByPromise.has(p5), 'Test 5: unhandledRejection should have fired');
    assert.strictEqual(unhandledByPromise.get(p5), err5);
  }));
}

// --- Test 6: Observe already-handled promise — no unhandledRejection ---
{
  const err6 = new Error('test6');
  const p6 = Promise.reject(err6);

  // Real handler added first.
  p6.catch(() => {});

  observePromise(p6, null, common.mustCall(() => {}));

  setImmediate(common.mustCall(() => {
    assert.ok(!unhandledByPromise.has(p6), 'Test 6: unhandledRejection should NOT have fired');
  }));
}
