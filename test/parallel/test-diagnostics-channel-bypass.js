'use strict';

const common = require('../common');
const assert = require('assert');
const { channel, bypass } = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');

// Test 1: Basic bypass - subscriber with bypassId is skipped inside bypass()
{
  const key = Symbol('tracer');
  const ch = channel('test-bypass-basic');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { bypassId: key });

  bypass(key, common.mustCall(() => {
    ch.publish({});
  }));

  ch.unsubscribe(handler);
}

// Test 2: Non-opted subscriber fires even inside bypass() scope
{
  const key = Symbol('tracer2');
  const ch = channel('test-bypass-nonopted');
  const optedHandler = common.mustNotCall();
  const regularHandler = common.mustCall();
  ch.subscribe(optedHandler, { bypassId: key });
  ch.subscribe(regularHandler); // no bypass

  bypass(key, common.mustCall(() => {
    ch.publish({});
  }));

  ch.unsubscribe(optedHandler);
  ch.unsubscribe(regularHandler);
}

// Test 3: Two APMs with different keys don't bypass each other
{
  const k1 = Symbol('k1');
  const k2 = Symbol('k2');
  const ch = channel('test-bypass-two-keys');
  let h1Calls = 0;
  let h2Calls = 0;
  const h1 = common.mustCall(() => { h1Calls++; }, 1);
  const h2 = common.mustCall(() => { h2Calls++; }, 1);
  ch.subscribe(h1, { bypassId: k1 });
  ch.subscribe(h2, { bypassId: k2 });

  bypass(k1, common.mustCall(() => {
    ch.publish({});
  }));

  assert.strictEqual(h1Calls, 0);
  assert.strictEqual(h2Calls, 1);

  bypass(k2, common.mustCall(() => {
    ch.publish({});
  }));

  assert.strictEqual(h1Calls, 1);
  assert.strictEqual(h2Calls, 1);

  ch.unsubscribe(h1);
  ch.unsubscribe(h2);
}

// Test 4: Nested bypass() calls (same key, different keys)
{
  const k1 = Symbol('nested1');
  const k2 = Symbol('nested2');
  const ch = channel('test-bypass-nested');
  const h1 = common.mustNotCall();
  let h2Calls = 0;
  const h2 = common.mustCall(() => { h2Calls++; }, 2);
  ch.subscribe(h1, { bypassId: k1 });
  ch.subscribe(h2, { bypassId: k2 });

  bypass(k1, common.mustCall(() => {
    // Inside k1, h1 skipped, h2 runs
    ch.publish({});
    assert.strictEqual(h2Calls, 1);

    bypass(k2, common.mustCall(() => {
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

// Test 5: bypass() across a Promise boundary
{
  const key = Symbol('promise');
  const ch = channel('test-bypass-promise');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { bypassId: key });
  const done = common.mustCall();

  bypass(key, common.mustCall(async () => {
    await Promise.resolve();
    ch.publish({});
  })).then(common.mustCall(() => {
    ch.unsubscribe(handler);
    done();
  }));
}

// Test 6: bypass() across setImmediate and queueMicrotask
{
  const key = Symbol('timers');
  const ch = channel('test-bypass-timers');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { bypassId: key });
  const done = common.mustCall();

  bypass(key, common.mustCall(async () => {
    await new Promise((resolve) => {
      setImmediate(common.mustCall(() => {
        ch.publish({});

        queueMicrotask(common.mustCall(() => {
          ch.publish({});
          resolve();
        }));
      }));
    });
  })).then(common.mustCall(() => {
    ch.unsubscribe(handler);
    done();
  }));
}

// Test 7: unsubscribe() works correctly after using bypassId
{
  const key = Symbol('unsub');
  const ch = channel('test-bypass-unsubscribe');
  const handler = common.mustNotCall();
  ch.subscribe(handler, { bypassId: key });
  ch.unsubscribe(handler);

  // Should not throw and should not be called
  bypass(key, common.mustCall(() => {
    ch.publish({});
  }));
}

// Test 8: bindStore with bypassId is skipped inside bypass()
{
  const key = Symbol('store');
  const ch = channel('test-bypass-store');
  const als = new AsyncLocalStorage();
  const normalAls = new AsyncLocalStorage();

  // Normal store, no bypassId, must always be entered.
  ch.bindStore(normalAls, common.mustCall((data) => ({ value: data.value })));

  // Bypass store, must NOT be entered inside bypass().
  ch.bindStore(als, common.mustNotCall(), { bypassId: key });

  // Handler verifies:
  // - normal store WAS entered (normalAls has value)
  // - bypass store was NOT entered (als is undefined)
  const handler = common.mustCall(() => {
    assert.strictEqual(normalAls.getStore()?.value, 42);
    assert.strictEqual(als.getStore(), undefined);
  });
  ch.subscribe(handler);

  bypass(key, common.mustCall(() => {
    ch.runStores({ value: 42 }, common.mustCall());
  }));

  ch.unsubscribe(handler);
  ch.unbindStore(als);
  ch.unbindStore(normalAls);
}

// Test 9: Wrong type for bypassId throws ERR_INVALID_ARG_TYPE
{
  const ch = channel('test-bypass-wrong-type');
  const bad = 'not-allowed';
  assert.throws(() => ch.subscribe(() => {}, { bypassId: bad }), {
    name: 'TypeError'
  });
  const als = new AsyncLocalStorage();
  assert.throws(() => ch.bindStore(als, (d) => d, { bypassId: bad }), {
    name: 'TypeError'
  });
}

// Test 10: bypass() return value passes through fn's return value
{
  const key = Symbol('return');
  const receiver = { value: 41 };
  const result = bypass(key, common.mustCall(function(a, b) {
    assert.strictEqual(this, receiver);
    assert.strictEqual(a, 'a');
    assert.strictEqual(b, 'b');
    return this.value + 1;
  }), receiver, 'a', 'b');
  assert.strictEqual(result, 42);
}
