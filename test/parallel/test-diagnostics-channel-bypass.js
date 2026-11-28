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

  bypass(key, common.mustCall(() => {
    ch.runStores({ value: 42 }, common.mustCall(() => {
      // Assertions inside runStores where store context exists
      assert.strictEqual(normalAls.getStore()?.value, 42);
      assert.strictEqual(als.getStore(), undefined);
    }));
  }));

  ch.unbindStore(als);
  ch.unbindStore(normalAls);
}

// Test 9: Wrong type for bypassId throws ERR_INVALID_ARG_TYPE
{
  const ch = channel('test-bypass-wrong-type');
  const bad = 'not-allowed';
  assert.throws(() => ch.subscribe(() => {}, { bypassId: bad }), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
  const als = new AsyncLocalStorage();
  assert.throws(() => ch.bindStore(als, (d) => d, { bypassId: bad }), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
  assert.throws(() => bypass(bad, () => {}), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
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

// Test 11: TracingChannel traceSync respects bypass
{
  const { tracingChannel } = require('node:diagnostics_channel');
  const key = Symbol('tracing-sync');
  const tc = tracingChannel('test-bypass-tracing-sync');
  const startHandler = common.mustNotCall();
  const endHandler = common.mustNotCall();
  const errorHandler = common.mustNotCall();

  tc.subscribe({
    start: startHandler,
    end: endHandler,
    error: errorHandler,
  }, { bypassId: key });

  bypass(key, common.mustCall(() => {
    tc.traceSync(common.mustCall(), {});
  }));

  tc.unsubscribe({
    start: startHandler,
    end: endHandler,
    error: errorHandler,
  });
}

// Test 12: TracingChannel tracePromise respects bypass
{
  const { tracingChannel } = require('node:diagnostics_channel');
  const key = Symbol('tracing-promise');
  const tc = tracingChannel('test-bypass-tracing-promise');
  const startHandler = common.mustNotCall();
  const endHandler = common.mustNotCall();
  const asyncStartHandler = common.mustNotCall();
  const asyncEndHandler = common.mustNotCall();
  const done = common.mustCall();

  tc.subscribe({
    start: startHandler,
    end: endHandler,
    asyncStart: asyncStartHandler,
    asyncEnd: asyncEndHandler,
  }, { bypassId: key });

  bypass(key, common.mustCall(() => {
    return tc.tracePromise(common.mustCall(() => Promise.resolve(42)), {});
  })).then(common.mustCall(() => {
    tc.unsubscribe({
      start: startHandler,
      end: endHandler,
      asyncStart: asyncStartHandler,
      asyncEnd: asyncEndHandler,
    });
    done();
  }));
}

// Test 13: TracingChannel traceCallback respects bypass
{
  const { tracingChannel } = require('node:diagnostics_channel');
  const key = Symbol('tracing-callback');
  const tc = tracingChannel('test-bypass-tracing-callback');
  const startHandler = common.mustNotCall();
  const endHandler = common.mustNotCall();
  const asyncStartHandler = common.mustNotCall();
  const asyncEndHandler = common.mustNotCall();
  const done = common.mustCall();

  tc.subscribe({
    start: startHandler,
    end: endHandler,
    asyncStart: asyncStartHandler,
    asyncEnd: asyncEndHandler,
  }, { bypassId: key });

  bypass(key, common.mustCall(() => {
    tc.traceCallback(
      common.mustCall((cb) => setImmediate(cb, null, 'result')),
      -1,
      {},
      null,
      common.mustCall((err, res) => {
        assert.strictEqual(err, null);
        assert.strictEqual(res, 'result');
        tc.unsubscribe({
          start: startHandler,
          end: endHandler,
          asyncStart: asyncStartHandler,
          asyncEnd: asyncEndHandler,
        });
        done();
      })
    );
  }));
}

// Test 14: Bind normally, rebind with bypassId, unbindStore once marks channel inactive
{
  const key = Symbol('rebind-normal-to-bypass');
  const ch = channel('test-bypass-rebind-normal-to-bypass');
  const als = new AsyncLocalStorage();

  ch.bindStore(als, (data) => data);
  assert.strictEqual(ch.hasSubscribers, true);

  // Rebind with bypassId
  ch.bindStore(als, (data) => data, { bypassId: key });
  assert.strictEqual(ch.hasSubscribers, true);

  // Outside bypass(key), the store is entered
  ch.runStores({ value: 1 }, common.mustCall(() => {
    assert.deepStrictEqual(als.getStore(), { value: 1 });
  }));

  // Inside bypass(key), the store is skipped
  bypass(key, common.mustCall(() => {
    ch.runStores({ value: 1 }, common.mustCall(() => {
      assert.strictEqual(als.getStore(), undefined);
    }));
  }));

  // Unbind once: must completely remove store and make channel inactive
  const unbound = ch.unbindStore(als);
  assert.strictEqual(unbound, true);
  assert.strictEqual(ch.hasSubscribers, false);

  // Store is no longer entered
  ch.runStores({ value: 1 }, common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
  }));

  // Unbinding again returns false
  assert.strictEqual(ch.unbindStore(als), false);
}

// Test 15: Bind with bypassId, rebind normally, unbindStore once marks channel inactive
{
  const key = Symbol('rebind-bypass-to-normal');
  const ch = channel('test-bypass-rebind-bypass-to-normal');
  const als = new AsyncLocalStorage();

  ch.bindStore(als, (data) => data, { bypassId: key });
  assert.strictEqual(ch.hasSubscribers, true);

  // Rebind without bypassId (normal)
  ch.bindStore(als, (data) => data);
  assert.strictEqual(ch.hasSubscribers, true);

  // Inside bypass(key), normal store should now fire (it was rebound to normal)
  bypass(key, common.mustCall(() => {
    ch.runStores({ value: 2 }, common.mustCall(() => {
      assert.deepStrictEqual(als.getStore(), { value: 2 });
    }));
  }));

  // Outside bypass(key), normal store fires
  ch.runStores({ value: 2 }, common.mustCall(() => {
    assert.deepStrictEqual(als.getStore(), { value: 2 });
  }));

  // Unbind once: must completely remove store and make channel inactive
  const unbound = ch.unbindStore(als);
  assert.strictEqual(unbound, true);
  assert.strictEqual(ch.hasSubscribers, false);

  // Store is no longer entered
  ch.runStores({ value: 3 }, common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
  }));

  // Unbinding again returns false
  assert.strictEqual(ch.unbindStore(als), false);
}

// Test 16: Store is entered exactly once per runStores() after rebind and transform calls verified
{
  const key = Symbol('rebind-single-entry');
  const ch = channel('test-bypass-rebind-single-entry');
  const als = new AsyncLocalStorage();

  let transformCalls = 0;
  ch.bindStore(als, (data) => {
    transformCalls++;
    return data;
  });

  // Rebind with bypassId
  let transformCallsRebound = 0;
  ch.bindStore(als, (data) => {
    transformCallsRebound++;
    return data;
  }, { bypassId: key });

  ch.runStores({ value: 42 }, common.mustCall(() => {
    assert.deepStrictEqual(als.getStore(), { value: 42 });
  }));

  // The original transform must not be called; the rebound transform called exactly once
  assert.strictEqual(transformCalls, 0);
  assert.strictEqual(transformCallsRebound, 1);

  // Rebind back to normal
  let transformCallsRebound2 = 0;
  ch.bindStore(als, (data) => {
    transformCallsRebound2++;
    return data;
  });

  ch.runStores({ value: 43 }, common.mustCall(() => {
    assert.deepStrictEqual(als.getStore(), { value: 43 });
  }));

  assert.strictEqual(transformCalls, 0);
  assert.strictEqual(transformCallsRebound, 1);
  assert.strictEqual(transformCallsRebound2, 1);

  ch.unbindStore(als);
  assert.strictEqual(ch.hasSubscribers, false);
}

// Test 17: Invalid bypassId on rebind throws ERR_INVALID_ARG_TYPE and preserves earlier binding and refcount
{
  const bad = 'invalid-string-key';

  // 1. Earlier binding is normal
  {
    const ch = channel('test-bypass-rebind-invalid-normal');
    const als = new AsyncLocalStorage();

    ch.bindStore(als, (data) => data);
    assert.strictEqual(ch.hasSubscribers, true);

    assert.throws(
      () => ch.bindStore(als, (data) => ({ modified: data }), { bypassId: bad }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );

    // Channel still active, earlier binding intact and entered
    assert.strictEqual(ch.hasSubscribers, true);
    ch.runStores({ value: 100 }, common.mustCall(() => {
      assert.deepStrictEqual(als.getStore(), { value: 100 });
    }));

    // Unbinding once still properly decrements refcount and marks inactive
    assert.strictEqual(ch.unbindStore(als), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }

  // 2. Earlier binding is bypass
  {
    const ch = channel('test-bypass-rebind-invalid-bypass');
    const als = new AsyncLocalStorage();
    const key = Symbol('valid-key');

    ch.bindStore(als, (data) => data, { bypassId: key });
    assert.strictEqual(ch.hasSubscribers, true);

    assert.throws(
      () => ch.bindStore(als, (data) => ({ modified: data }), { bypassId: bad }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );

    // Channel still active, earlier bypass binding intact
    assert.strictEqual(ch.hasSubscribers, true);

    // Outside bypass, store is entered
    ch.runStores({ value: 200 }, common.mustCall(() => {
      assert.deepStrictEqual(als.getStore(), { value: 200 });
    }));

    // Inside bypass with valid key, store is skipped
    bypass(key, common.mustCall(() => {
      ch.runStores({ value: 200 }, common.mustCall(() => {
        assert.strictEqual(als.getStore(), undefined);
      }));
    }));

    // Unbinding once still properly decrements refcount and marks inactive
    assert.strictEqual(ch.unbindStore(als), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }
}

// Test 18: Invalid bypassId on fresh inactive channel leaves channel inactive
{
  const bad = 'invalid-string-key';

  // 1. Invalid bypassId passed to subscribe() on fresh inactive channel
  {
    const ch = channel('test-bypass-fresh-invalid-subscribe');
    assert.strictEqual(ch.hasSubscribers, false);

    assert.throws(
      () => ch.subscribe(() => {}, { bypassId: bad }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
    assert.strictEqual(ch.hasSubscribers, false);

    const key = Symbol('valid-subscribe');
    const handler = common.mustCall();
    ch.subscribe(handler, { bypassId: key });
    assert.strictEqual(ch.hasSubscribers, true);

    ch.publish({});

    assert.strictEqual(ch.unsubscribe(handler), true);
    assert.strictEqual(ch.hasSubscribers, false);

    const plainHandler = common.mustCall();
    ch.subscribe(plainHandler);
    assert.strictEqual(ch.hasSubscribers, true);

    ch.publish({});

    assert.strictEqual(ch.unsubscribe(plainHandler), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }

  // 2. Invalid bypassId passed to bindStore() on fresh inactive channel
  {
    const ch = channel('test-bypass-fresh-invalid-bind-store');
    const als = new AsyncLocalStorage();
    assert.strictEqual(ch.hasSubscribers, false);

    assert.throws(
      () => ch.bindStore(als, (data) => data, { bypassId: bad }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
    assert.strictEqual(ch.hasSubscribers, false);

    const key = Symbol('valid-bind-store');
    const handler = common.mustCall();
    ch.subscribe(handler, { bypassId: key });
    assert.strictEqual(ch.hasSubscribers, true);

    ch.publish({});

    assert.strictEqual(ch.unsubscribe(handler), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }
}

// Test 19: bypass subscribers and stores run after regular ones, regardless of registration order (documented behavior)
{
  const key = Symbol('ordering');

  // Subscribers
  {
    const ch = channel('test-bypass-order-subscribers');
    const order = [];
    const bypassHandler1 = common.mustCall(() => { order.push('bypass1'); });
    const regular1 = common.mustCall(() => { order.push('regular1'); });
    const bypassHandler2 = common.mustCall(() => { order.push('bypass2'); });
    const regular2 = common.mustCall(() => { order.push('regular2'); });

    ch.subscribe(bypassHandler1, { bypassId: key });
    ch.subscribe(regular1);
    ch.subscribe(bypassHandler2, { bypassId: key });
    ch.subscribe(regular2);

    ch.publish({});

    assert.deepStrictEqual(order, ['regular1', 'regular2', 'bypass1', 'bypass2']);

    assert.strictEqual(ch.unsubscribe(bypassHandler1), true);
    assert.strictEqual(ch.unsubscribe(regular1), true);
    assert.strictEqual(ch.unsubscribe(bypassHandler2), true);
    assert.strictEqual(ch.unsubscribe(regular2), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }

  // Stores
  {
    const ch = channel('test-bypass-order-stores');
    const order = [];
    const bypassStore = new AsyncLocalStorage();
    const regularStore1 = new AsyncLocalStorage();
    const regularStore2 = new AsyncLocalStorage();

    ch.bindStore(bypassStore, common.mustCall((data) => {
      order.push('bypass');
      return data;
    }), { bypassId: key });
    ch.bindStore(regularStore1, common.mustCall((data) => {
      order.push('regular1');
      return data;
    }));
    ch.bindStore(regularStore2, common.mustCall((data) => {
      order.push('regular2');
      return data;
    }));

    ch.runStores({}, common.mustCall());

    assert.deepStrictEqual(order, ['regular1', 'regular2', 'bypass']);

    assert.strictEqual(ch.unbindStore(bypassStore), true);
    assert.strictEqual(ch.unbindStore(regularStore1), true);
    assert.strictEqual(ch.unbindStore(regularStore2), true);
    assert.strictEqual(ch.hasSubscribers, false);
  }
}

// Test 20: object keys work like symbol keys
{
  const key = {};
  const ch = channel('test-bypass-object-key');
  const als = new AsyncLocalStorage();
  const handler = common.mustNotCall();
  ch.subscribe(handler, { bypassId: key });
  ch.bindStore(als, common.mustNotCall(), { bypassId: key });
  bypass(key, common.mustCall(() => {
    ch.publish({});
    ch.runStores({}, common.mustCall());
  }));
  assert.strictEqual(ch.unsubscribe(handler), true);
  assert.strictEqual(ch.unbindStore(als), true);
  assert.strictEqual(ch.hasSubscribers, false);
}
