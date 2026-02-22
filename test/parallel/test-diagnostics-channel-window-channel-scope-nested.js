/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test nested scopes
{
  const windowChannel = dc.windowChannel('test-nested-basic');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', id: message.id });
    },
    end(message) {
      events.push({ type: 'end', id: message.id });
    },
  });

  {
    using outer = windowChannel.withScope({ id: 'outer' });
    events.push({ type: 'work', id: 'outer' });

    {
      using inner = windowChannel.withScope({ id: 'inner' });
      events.push({ type: 'work', id: 'inner' });
    }

    events.push({ type: 'work', id: 'outer-after' });
  }

  assert.strictEqual(events.length, 7);
  assert.deepStrictEqual(events[0], { type: 'start', id: 'outer' });
  assert.deepStrictEqual(events[1], { type: 'work', id: 'outer' });
  assert.deepStrictEqual(events[2], { type: 'start', id: 'inner' });
  assert.deepStrictEqual(events[3], { type: 'work', id: 'inner' });
  assert.deepStrictEqual(events[4], { type: 'end', id: 'inner' });
  assert.deepStrictEqual(events[5], { type: 'work', id: 'outer-after' });
  assert.deepStrictEqual(events[6], { type: 'end', id: 'outer' });
}

// Test nested scopes with stores
{
  const windowChannel = dc.windowChannel('test-nested-stores');
  const store = new AsyncLocalStorage();
  const storeValues = [];

  windowChannel.start.bindStore(store, (context) => context.id);

  windowChannel.subscribe({
    start() {},
    end() {},
  });

  assert.strictEqual(store.getStore(), undefined);

  {
    using outer = windowChannel.withScope({ id: 'outer' });
    storeValues.push(store.getStore());

    {
      using inner = windowChannel.withScope({ id: 'inner' });
      storeValues.push(store.getStore());
    }

    // Should restore to outer
    storeValues.push(store.getStore());
  }

  // Should restore to undefined
  storeValues.push(store.getStore());

  assert.deepStrictEqual(storeValues, ['outer', 'inner', 'outer', undefined]);
}

// Test nested scopes with different channels
{
  const channel1 = dc.windowChannel('test-nested-chan1');
  const channel2 = dc.windowChannel('test-nested-chan2');
  const events = [];

  channel1.subscribe({
    start({ ...data }) {
      events.push({ channel: 1, type: 'start', data });
    },
    end({ ...data }) {
      events.push({ channel: 1, type: 'end', data });
    },
  });

  channel2.subscribe({
    start({ ...data }) {
      events.push({ channel: 2, type: 'start', data });
    },
    end({ ...data }) {
      events.push({ channel: 2, type: 'end', data });
    },
  });

  const contextA = { id: 'A' };
  const contextB = { id: 'B' };
  {
    using scope1 = channel1.withScope(contextA);

    {
      using scope2 = channel2.withScope(contextB);
      contextB.result = 'B-result';
    }

    contextA.result = 'A-result';
  }

  assert.strictEqual(events.length, 4);
  assert.deepStrictEqual(events, [
    { channel: 1, type: 'start', data: { id: 'A' } },
    { channel: 2, type: 'start', data: { id: 'B' } },
    { channel: 2, type: 'end', data: { id: 'B', result: 'B-result' } },
    { channel: 1, type: 'end', data: { id: 'A', result: 'A-result' } },
  ]);
}

// Test nested scopes with shared store
{
  const channel1 = dc.windowChannel('test-nested-shared1');
  const channel2 = dc.windowChannel('test-nested-shared2');
  const store = new AsyncLocalStorage();
  const storeValues = [];

  channel1.start.bindStore(store, (context) => ({ from: 'channel1', ...context }));
  channel2.start.bindStore(store, (context) => ({ from: 'channel2', ...context }));

  channel1.subscribe({ start() {}, end() {} });
  channel2.subscribe({ start() {}, end() {} });

  {
    using scope1 = channel1.withScope({ id: 1 });
    storeValues.push({ ...store.getStore() });

    {
      using scope2 = channel2.withScope({ id: 2 });
      storeValues.push({ ...store.getStore() });
    }

    // Should restore to channel1's store value
    storeValues.push({ ...store.getStore() });
  }

  assert.strictEqual(storeValues.length, 3);
  assert.deepStrictEqual(storeValues[0], { from: 'channel1', id: 1 });
  assert.deepStrictEqual(storeValues[1], { from: 'channel2', id: 2 });
  assert.deepStrictEqual(storeValues[2], { from: 'channel1', id: 1 });
}

// Test deeply nested scopes
{
  const windowChannel = dc.windowChannel('test-nested-deep');
  const store = new AsyncLocalStorage();
  const depths = [];

  windowChannel.start.bindStore(store, (context) => context.depth);

  windowChannel.subscribe({
    start() {},
    end() {},
  });

  {
    using s1 = windowChannel.withScope({ depth: 1 });
    depths.push(store.getStore());

    {
      using s2 = windowChannel.withScope({ depth: 2 });
      depths.push(store.getStore());

      {
        using s3 = windowChannel.withScope({ depth: 3 });
        depths.push(store.getStore());

        {
          using s4 = windowChannel.withScope({ depth: 4 });
          depths.push(store.getStore());
        }

        depths.push(store.getStore());
      }

      depths.push(store.getStore());
    }

    depths.push(store.getStore());
  }

  depths.push(store.getStore());

  assert.deepStrictEqual(depths, [1, 2, 3, 4, 3, 2, 1, undefined]);
}

// Test nested scopes with errors
{
  const windowChannel = dc.windowChannel('test-nested-error');
  const store = new AsyncLocalStorage();
  const events = [];

  windowChannel.start.bindStore(store, (context) => context.id);

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', id: message.id });
    },
    end(message) {
      events.push({ type: 'end', id: message.id });
    },
  });

  const testError = new Error('inner error');

  assert.throws(() => {
    using outer = windowChannel.withScope({ id: 'outer' });
    events.push({ type: 'store', value: store.getStore() });

    assert.throws(() => {
      using inner = windowChannel.withScope({ id: 'inner' });
      events.push({ type: 'store', value: store.getStore() });
      throw testError;
    }, testError);

    // After inner error, should be back to outer store
    events.push({ type: 'store', value: store.getStore() });

    throw new Error('outer error');
  }, /outer error/);

  // Both start and end events should have been published for both scopes
  assert.strictEqual(events[0].type, 'start');
  assert.strictEqual(events[0].id, 'outer');
  assert.strictEqual(events[1].type, 'store');
  assert.strictEqual(events[1].value, 'outer');

  assert.strictEqual(events[2].type, 'start');
  assert.strictEqual(events[2].id, 'inner');
  assert.strictEqual(events[3].type, 'store');
  assert.strictEqual(events[3].value, 'inner');

  assert.strictEqual(events[4].type, 'end');
  assert.strictEqual(events[4].id, 'inner');

  assert.strictEqual(events[5].type, 'store');
  assert.strictEqual(events[5].value, 'outer');

  assert.strictEqual(events[6].type, 'end');
  assert.strictEqual(events[6].id, 'outer');

  // Store should be restored
  assert.strictEqual(store.getStore(), undefined);
}
