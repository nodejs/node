/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test nested scopes
{
  const boundedChannel = dc.boundedChannel('test-nested-basic');
  const events = [];

  boundedChannel.subscribe({
    start(message) {
      events.push({ type: 'start', id: message.id });
    },
    end(message) {
      events.push({ type: 'end', id: message.id });
    },
  });

  {
    using outer = boundedChannel.withScope({ id: 'outer' });
    events.push({ type: 'work', id: 'outer' });

    {
      using inner = boundedChannel.withScope({ id: 'inner' });
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
  const boundedChannel = dc.boundedChannel('test-nested-stores');
  const store = new AsyncLocalStorage();
  const storeValues = [];

  boundedChannel.start.bindStore(store, (context) => context.id);

  boundedChannel.subscribe({
    start() {},
    end() {},
  });

  assert.strictEqual(store.getStore(), undefined);

  {
    using outer = boundedChannel.withScope({ id: 'outer' });
    storeValues.push(store.getStore());

    {
      using inner = boundedChannel.withScope({ id: 'inner' });
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
  const channel1 = dc.boundedChannel('test-nested-chan1');
  const channel2 = dc.boundedChannel('test-nested-chan2');
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
  const channel1 = dc.boundedChannel('test-nested-shared1');
  const channel2 = dc.boundedChannel('test-nested-shared2');
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
  const boundedChannel = dc.boundedChannel('test-nested-deep');
  const store = new AsyncLocalStorage();
  const depths = [];

  boundedChannel.start.bindStore(store, (context) => context.depth);

  boundedChannel.subscribe({
    start() {},
    end() {},
  });

  {
    using s1 = boundedChannel.withScope({ depth: 1 });
    depths.push(store.getStore());

    {
      using s2 = boundedChannel.withScope({ depth: 2 });
      depths.push(store.getStore());

      {
        using s3 = boundedChannel.withScope({ depth: 3 });
        depths.push(store.getStore());

        {
          using s4 = boundedChannel.withScope({ depth: 4 });
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
  const boundedChannel = dc.boundedChannel('test-nested-error');
  const store = new AsyncLocalStorage();
  const events = [];

  boundedChannel.start.bindStore(store, (context) => context.id);

  boundedChannel.subscribe({
    start(message) {
      events.push({ type: 'start', id: message.id });
    },
    end(message) {
      events.push({ type: 'end', id: message.id });
    },
  });

  const testError = new Error('inner error');

  assert.throws(() => {
    using outer = boundedChannel.withScope({ id: 'outer' });
    events.push({ type: 'store', value: store.getStore() });

    assert.throws(() => {
      using inner = boundedChannel.withScope({ id: 'inner' });
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
