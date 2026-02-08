/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test basic scope with using
{
  const windowChannel = dc.windowChannel('test-scope-basic');
  const events = [];

  windowChannel.subscribe({
    start({ ...data }) {
      events.push({ type: 'start', data });
    },
    end({ ...data }) {
      events.push({ type: 'end', data });
    },
  });

  const context = { id: 123 };

  {
    using scope = windowChannel.withScope(context);
    assert.ok(scope);
    context.value = 'inside';
  }

  assert.strictEqual(events.length, 2);
  assert.deepStrictEqual(events, [
    {
      type: 'start',
      data: { id: 123 }
    },
    {
      type: 'end',
      data: {
        id: 123,
        value: 'inside'
      }
    },
  ]);
}

// Test scope with result setter
{
  const windowChannel = dc.windowChannel('test-scope-result');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', data: message });
    },
    end(message) {
      events.push({ type: 'end', data: message });
    },
  });

  const context = {};

  {
    using scope = windowChannel.withScope(context);
    context.result = 42;
  }

  assert.strictEqual(context.result, 42);
  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[1].data.result, 42);
}

// Test scope with error setter
{
  const windowChannel = dc.windowChannel('test-scope-error-setter');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', data: message });
    },
    end(message) {
      events.push({ type: 'end', data: message });
    },
  });

  const context = {};

  {
    using scope = windowChannel.withScope(context);
    context.result = 'test result';
  }

  // WindowChannel does not handle errors - only start and end
  assert.strictEqual(context.result, 'test result');
  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[0].type, 'start');
  assert.strictEqual(events[1].type, 'end');
}

// Test scope with AsyncLocalStorage
{
  const windowChannel = dc.windowChannel('test-scope-store');
  const store = new AsyncLocalStorage();
  const events = [];

  windowChannel.start.bindStore(store, (context) => {
    return { traceId: context.traceId };
  });

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', store: store.getStore() });
    },
    end(message) {
      events.push({ type: 'end', store: store.getStore() });
    },
  });

  assert.strictEqual(store.getStore(), undefined);

  {
    using scope = windowChannel.withScope({ traceId: 'xyz789' });

    // Store should be set inside scope
    assert.deepStrictEqual(store.getStore(), { traceId: 'xyz789' });

    events.push({ type: 'inside', store: store.getStore() });
  }

  // Store should be restored after scope
  assert.strictEqual(store.getStore(), undefined);

  assert.strictEqual(events.length, 3);
  assert.strictEqual(events[0].type, 'start');
  assert.deepStrictEqual(events[0].store, { traceId: 'xyz789' });
  assert.strictEqual(events[1].type, 'inside');
  assert.deepStrictEqual(events[1].store, { traceId: 'xyz789' });
  assert.strictEqual(events[2].type, 'end');
  assert.deepStrictEqual(events[2].store, { traceId: 'xyz789' });
}

// Test scope without subscribers (no-op)
{
  const windowChannel = dc.windowChannel('test-scope-no-subs');

  const context = {};

  {
    using scope = windowChannel.withScope(context);
    context.result = 'value';
  }

  // Context should still be updated even without subscribers
  assert.strictEqual(context.result, 'value');
}

// Test manual dispose via Symbol.dispose
{
  const windowChannel = dc.windowChannel('test-scope-manual');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push('start');
    },
    end(message) {
      events.push('end');
    },
  });

  const scope = windowChannel.withScope({});
  assert.strictEqual(events.length, 1);
  assert.strictEqual(events[0], 'start');

  scope[Symbol.dispose]();
  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[1], 'end');

  // Double dispose should be idempotent
  scope[Symbol.dispose]();
  assert.strictEqual(events.length, 2);
}

// Test scope with store restore to previous value
{
  const windowChannel = dc.windowChannel('test-scope-restore');
  const store = new AsyncLocalStorage();

  windowChannel.start.bindStore(store, (context) => context.value);

  windowChannel.subscribe({
    start() {},
    end() {},
  });

  store.enterWith('initial');
  assert.strictEqual(store.getStore(), 'initial');

  {
    using scope = windowChannel.withScope({ value: 'scoped' });
    assert.strictEqual(store.getStore(), 'scoped');
  }

  // Should restore to previous value
  assert.strictEqual(store.getStore(), 'initial');
}
