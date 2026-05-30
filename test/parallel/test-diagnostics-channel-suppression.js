'use strict';

const common = require('../common');
const assert = require('assert');
const { channel, suppressed } = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');

// Test 1: Basic suppression - subscriber with subscriberId is skipped inside suppressed()
{
  const key = Symbol('tracer');
  const ch = channel('test-suppression-basic');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { subscriberId: key });

  suppressed(key, () => {
    ch.publish({});
  });

  ch.unsubscribe(handler);
}

// Test 2: Non-opted subscriber fires even inside suppressed() scope
{
  const key = Symbol('tracer2');
  const ch = channel('test-suppression-nonopted');
  const optedHandler = common.mustNotCall();
  const regularHandler = common.mustCall();
  ch.subscribe(optedHandler, { subscriberId: key });
  ch.subscribe(regularHandler); // no suppression

  suppressed(key, () => {
    ch.publish({});
  });

  ch.unsubscribe(optedHandler);
  ch.unsubscribe(regularHandler);
}

// Test 3: Two APMs with different keys don't suppress each other
{
  const k1 = Symbol('k1');
  const k2 = Symbol('k2');
  const ch = channel('test-suppression-two-keys');
  let h1Calls = 0;
  let h2Calls = 0;
  const h1 = common.mustCall(() => { h1Calls++; }, 1);
  const h2 = common.mustCall(() => { h2Calls++; }, 1);
  ch.subscribe(h1, { subscriberId: k1 });
  ch.subscribe(h2, { subscriberId: k2 });

  suppressed(k1, () => {
    ch.publish({});
  });

  assert.strictEqual(h1Calls, 0);
  assert.strictEqual(h2Calls, 1);

  suppressed(k2, () => {
    ch.publish({});
  });

  assert.strictEqual(h1Calls, 1);
  assert.strictEqual(h2Calls, 1);

  ch.unsubscribe(h1);
  ch.unsubscribe(h2);
}

// Test 4: Nested suppressed() calls (same key, different keys)
{
  const k1 = Symbol('nested1');
  const k2 = Symbol('nested2');
  const ch = channel('test-suppression-nested');
  const h1 = common.mustNotCall();
  let h2Calls = 0;
  const h2 = common.mustCall(() => { h2Calls++; }, 2);
  ch.subscribe(h1, { subscriberId: k1 });
  ch.subscribe(h2, { subscriberId: k2 });

  suppressed(k1, common.mustSucceed(() => {
    // Inside k1, h1 skipped, h2 runs
    ch.publish({});
    assert.strictEqual(h2Calls, 1);

    suppressed(k2, common.mustSucceed(() => {
      // Inside both, both skipped
      ch.publish({});
      assert.strictEqual(h2Calls, 1);
    }));

    // Back to only k1
    ch.publish({});
    assert.strictEqual(h2Calls, 2);
  }));

  ch.unsubscribe(h1);
  ch.unsubscribe(h2);
}

// Test 5: suppressed() across a Promise boundary
{
  const key = Symbol('promise');
  const ch = channel('test-suppression-promise');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { subscriberId: key });

  suppressed(key, async () => {
    await Promise.resolve();
    ch.publish({});
  }).then(common.mustCall(() => {
    ch.unsubscribe(handler);
  }));
}

// Test 6: suppressed() across setImmediate and queueMicrotask
{
  const key = Symbol('timers');
  const ch = channel('test-suppression-timers');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { subscriberId: key });

  suppressed(key, async () => {
    await new Promise((resolve) => {
      setImmediate(() => {
        ch.publish({});

        queueMicrotask(() => {
          ch.publish({});
          resolve();
        });
      });
    });
  }).then(common.mustCall(() => {
    ch.unsubscribe(handler);
  }));
}

// Test 7: unsubscribe() works correctly after using subscriberId
{
  const key = Symbol('unsub');
  const ch = channel('test-suppression-unsubscribe');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { subscriberId: key });
  ch.unsubscribe(handler);

  // Should not throw and should not be called
  suppressed(key, () => {
    ch.publish({});
  });

}

// Test 8: bindStore with subscriberId is skipped inside suppressed()
{
  const key = Symbol('store');
  const ch = channel('test-suppression-store');
  const als = new AsyncLocalStorage();

  const handler = common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
  });

  ch.subscribe(handler);
  ch.bindStore(als, common.mustNotCall(), { subscriberId: key });

  suppressed(key, () => {
    ch.publish({});
  });

  ch.unsubscribe(handler);
  ch.unbindStore(als);
}

// Test 9: Wrong type for subscriberId throws ERR_INVALID_ARG_TYPE
{
  const ch = channel('test-suppression-wrong-type');
  const bad = 'not-allowed';
  assert.throws(() => ch.subscribe(() => {}, { subscriberId: bad }), {
    name: 'TypeError'
  });
  const als = new AsyncLocalStorage();
  assert.throws(() => ch.bindStore(als, (d) => d, { subscriberId: bad }), {
    name: 'TypeError'
  });
}

// Test 10: suppressed() return value passes through fn's return value
{
  const key = Symbol('return');
  const receiver = { value: 41 };
  const result = suppressed(key, common.mustSucceed(function(a, b) {
    assert.strictEqual(this, receiver);
    assert.strictEqual(a, 'a');
    assert.strictEqual(b, 'b');
    return this.value + 1;
  }), receiver, 'a', 'b');
  assert.strictEqual(result, 42);
}

console.log('ok - diagnostics_channel suppression tests loaded');
