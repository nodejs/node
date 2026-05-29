'use strict';

const common = require('../common');
const assert = require('assert');
const { channel, suppressed } = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');

// Helper to run a function and capture whether a handler was called
function makeHandler() {
  let called = false;
  const handler = (msg) => { called = true; };
  return {
    handler,
    called: () => called,
    reset: () => { called = false; }
  };
}

// Test 1: Basic suppression - subscriber with suppressedBy is skipped inside suppressed()
(function testBasicSuppression() {
  const key = Symbol('tracer');
  const ch = channel('test-suppression-basic');
  const h = makeHandler();
  ch.subscribe(h.handler, { suppressedBy: key });

  suppressed(key, () => {
    ch.publish({});
  });

  assert.strictEqual(h.called(), false, 'suppressed subscriber should not be called');
  // cleanup
  ch.unsubscribe(h.handler);
})();

// Test 2: Non-opted subscriber fires even inside suppressed() scope
(function testNonOptedFires() {
  const key = Symbol('tracer2');
  const ch = channel('test-suppression-nonopted');
  const h1 = makeHandler();
  const h2 = makeHandler();
  ch.subscribe(h1.handler, { suppressedBy: key });
  ch.subscribe(h2.handler); // no suppression

  suppressed(key, () => {
    ch.publish({});
  });

  assert.strictEqual(h1.called(), false, 'opted subscriber should be skipped');
  assert.strictEqual(h2.called(), true, 'non-opted subscriber should be called');

  ch.unsubscribe(h1.handler);
  ch.unsubscribe(h2.handler);
})();

// Test 3: Two APMs with different keys don't suppress each other
(function testTwoKeysIndependent() {
  const k1 = Symbol('k1');
  const k2 = Symbol('k2');
  const ch = channel('test-suppression-two-keys');
  const h1 = makeHandler();
  const h2 = makeHandler();
  ch.subscribe(h1.handler, { suppressedBy: k1 });
  ch.subscribe(h2.handler, { suppressedBy: k2 });

  suppressed(k1, () => {
    ch.publish({});
  });

  assert.strictEqual(h1.called(), false);
  assert.strictEqual(h2.called(), true);

  h1.reset(); h2.reset();

  suppressed(k2, () => {
    ch.publish({});
  });

  assert.strictEqual(h1.called(), true);
  assert.strictEqual(h2.called(), false);

  ch.unsubscribe(h1.handler);
  ch.unsubscribe(h2.handler);
})();

// Test 4: Nested suppressed() calls (same key, different keys)
(function testNestedSuppressed() {
  const k1 = Symbol('nested1');
  const k2 = Symbol('nested2');
  const ch = channel('test-suppression-nested');
  const h1 = makeHandler();
  const h2 = makeHandler();
  ch.subscribe(h1.handler, { suppressedBy: k1 });
  ch.subscribe(h2.handler, { suppressedBy: k2 });

  suppressed(k1, () => {
    // inside k1, h1 skipped, h2 runs
    ch.publish({});
    assert.strictEqual(h1.called(), false);
    assert.strictEqual(h2.called(), true);
    h2.reset();

    suppressed(k2, () => {
      // inside both, both skipped
      ch.publish({});
      assert.strictEqual(h1.called(), false);
      assert.strictEqual(h2.called(), false);
    });

    // back to only k1
    ch.publish({});
    assert.strictEqual(h1.called(), false);
    assert.strictEqual(h2.called(), true);
  });

  ch.unsubscribe(h1.handler);
  ch.unsubscribe(h2.handler);
})();

// Test 5: suppressed() across a Promise boundary (async/await)
(async function testSuppressedAcrossPromise() {
  const key = Symbol('promise');
  const ch = channel('test-suppression-promise');
  const h = makeHandler();
  ch.subscribe(h.handler, { suppressedBy: key });

  await suppressed(key, async () => {
    await Promise.resolve();
    ch.publish({});
  });

  assert.strictEqual(h.called(), false);
  ch.unsubscribe(h.handler);
})();

// Test 6: suppressed() across setImmediate and queueMicrotask
(async function testSuppressedAcrossTimers() {
  const key = Symbol('timers');
  const ch = channel('test-suppression-timers');
  const h = makeHandler();
  ch.subscribe(h.handler, { suppressedBy: key });

  await suppressed(key, async () => {
    await new Promise((resolve) => {
      setImmediate(() => {
        ch.publish({});
        assert.strictEqual(h.called(), false);
        h.reset();

        queueMicrotask(() => {
          ch.publish({});
          assert.strictEqual(h.called(), false);

          ch.unsubscribe(h.handler);
          resolve();
        });
      });
    });
  });
})();

// Test 7: unsubscribe() works correctly after using suppressedBy
(function testUnsubscribeCleansUp() {
  const key = Symbol('unsub');
  const ch = channel('test-suppression-unsubscribe');
  const h = makeHandler();
  ch.subscribe(h.handler, { suppressedBy: key });
  ch.unsubscribe(h.handler);

  // Should not throw and should not be called
  suppressed(key, () => {
    ch.publish({});
  });

  assert.strictEqual(h.called(), false);
})();

// Test 8: bindStore with suppressedBy is skipped inside suppressed()
(function testBindStoreSuppression() {
  const key = Symbol('store');
  const ch = channel('test-suppression-store');
  const als = new AsyncLocalStorage();

  let transformCalls = 0;
  const handler = common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
  });

  ch.subscribe(handler);
  ch.bindStore(als, (d) => {
    transformCalls++;
    return { foo: d };
  }, { suppressedBy: key });

  suppressed(key, () => {
    ch.publish({});
  });

  assert.strictEqual(transformCalls, 0);
  ch.unsubscribe(handler);
  ch.unbindStore(als);
})();

// Test 9: Wrong type for suppressedBy throws ERR_INVALID_ARG_TYPE
(function testWrongTypeThrows() {
  const ch = channel('test-suppression-wrong-type');
  const bad = 'not-allowed';
  assert.throws(() => ch.subscribe(() => {}, { suppressedBy: bad }), {
    name: 'TypeError'
  });
  const als = new AsyncLocalStorage();
  assert.throws(() => ch.bindStore(als, (d) => d, { suppressedBy: bad }), {
    name: 'TypeError'
  });
})();

// Test 10: suppressed() return value passes through fn's return value
(function testSuppressedReturnValueAndContext() {
  const key = Symbol('return');
  const receiver = { value: 41 };
  const result = suppressed(key, function(a, b) {
    assert.strictEqual(this, receiver);
    assert.strictEqual(a, 'a');
    assert.strictEqual(b, 'b');
    return this.value + 1;
  }, receiver, 'a', 'b');
  assert.strictEqual(result, 42);
})();

// Test 11: null suppressedBy behaves like no suppression opt-in
(function testNullSuppressedByIsIgnored() {
  const key = Symbol('null-suppressed-by');
  const ch = channel('test-suppression-null-suppressed-by');
  const h = makeHandler();

  ch.subscribe(h.handler, { suppressedBy: null });

  suppressed(key, () => {
    ch.publish({});
  });

  assert.strictEqual(h.called(), true);
  ch.unsubscribe(h.handler);
})();

// Test 12: suppressed() rejects null/undefined keys consistently
(function testKeyValidation() {
  assert.throws(() => suppressed(null, () => {}), {
    name: 'TypeError'
  });
  assert.throws(() => suppressed(undefined, () => {}), {
    name: 'TypeError'
  });
})();

console.log('ok - diagnostics_channel suppression tests loaded');
