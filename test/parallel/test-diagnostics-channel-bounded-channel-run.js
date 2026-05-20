'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test basic run functionality
{
  const boundedChannel = dc.boundedChannel('test-run-basic');
  const events = [];

  boundedChannel.subscribe({
    start(message) {
      events.push({ type: 'start', data: message });
    },
    end(message) {
      events.push({ type: 'end', data: message });
    },
  });

  const context = { id: 123 };
  const result = boundedChannel.run(context, () => {
    return 'success';
  });

  assert.strictEqual(result, 'success');
  assert.strictEqual(events.length, 2);
  assert.deepStrictEqual(events, [
    { type: 'start', data: { id: 123 } },
    { type: 'end', data: { id: 123 } },
  ]);
}

// Test run with error
{
  const boundedChannel = dc.boundedChannel('test-run-error');
  const events = [];

  boundedChannel.subscribe({
    start(message) {
      events.push({ type: 'start', data: message });
    },
    end(message) {
      events.push({ type: 'end', data: message });
    },
  });

  const context = { id: 456 };
  const testError = new Error('test error');

  assert.throws(() => {
    boundedChannel.run(context, () => {
      throw testError;
    });
  }, testError);

  // BoundedChannel does not handle errors - they just propagate
  // Only start and end events are published
  assert.strictEqual(events.length, 2);
  assert.deepStrictEqual(events, [
    { type: 'start', data: { id: 456 } },
    { type: 'end', data: { id: 456 } },
  ]);
}

// Test run with thisArg and args
{
  const boundedChannel = dc.boundedChannel('test-run-args');

  const obj = { value: 10 };
  const result = boundedChannel.run({}, function(a, b) {
    return this.value + a + b;
  }, obj, 5, 15);

  assert.strictEqual(result, 30);
}

// Test run with AsyncLocalStorage
{
  const boundedChannel = dc.boundedChannel('test-run-store');
  const store = new AsyncLocalStorage();
  const events = [];

  boundedChannel.start.bindStore(store, (context) => {
    return { traceId: context.traceId };
  });

  boundedChannel.subscribe({
    start(message) {
      events.push({ type: 'start', store: store.getStore() });
    },
    end(message) {
      events.push({ type: 'end', store: store.getStore() });
    },
  });

  const result = boundedChannel.run({ traceId: 'abc123' }, () => {
    events.push({ type: 'inside', store: store.getStore() });
    return 'result';
  });

  assert.strictEqual(result, 'result');
  assert.strictEqual(events.length, 3);

  // Innert events should have store set
  assert.deepStrictEqual(events, [
    { type: 'start', store: { traceId: 'abc123' } },
    { type: 'inside', store: { traceId: 'abc123' } },
    { type: 'end', store: { traceId: 'abc123' } },
  ]);

  // Store should be undefined outside
  assert.strictEqual(store.getStore(), undefined);
}

// Test run without subscribers
{
  const boundedChannel = dc.boundedChannel('test-run-no-subs');

  const result = boundedChannel.run({}, () => {
    return 'fast path';
  });

  assert.strictEqual(result, 'fast path');
}
