'use strict';
const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test WindowChannel.run() with store transform error
// Transform errors are scheduled via process.nextTick(triggerUncaughtException)

const windowChannel = dc.windowChannel('test-run-transform-error');
const store = new AsyncLocalStorage();
const events = [];

const transformError = new Error('transform failed');

// Set up uncaughtException handler to catch the transform error
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, transformError);
  events.push('uncaughtException');
}));

windowChannel.subscribe({
  start(message) {
    events.push({ type: 'start', data: message });
  },
  end(message) {
    events.push({ type: 'end', data: message });
  },
});

// Bind store with a transform that throws
windowChannel.start.bindStore(store, () => {
  throw transformError;
});

// Store should remain undefined since transform will fail
assert.strictEqual(store.getStore(), undefined);

const result = windowChannel.run({ operationId: '123' }, common.mustCall(() => {
  // Store should still be undefined because transform threw
  assert.strictEqual(store.getStore(), undefined);

  events.push('inside-run');

  return 42;
}));

// Should still return the result despite transform error
assert.strictEqual(result, 42);

// Store should still be undefined after run
assert.strictEqual(store.getStore(), undefined);

// Start and end events should still be published despite transform error
assert.strictEqual(events.length, 3);
assert.strictEqual(events[0].type, 'start');
assert.strictEqual(events[0].data.operationId, '123');
assert.strictEqual(events[1], 'inside-run');
assert.strictEqual(events[2].type, 'end');
assert.strictEqual(events[2].data.operationId, '123');

// Validate uncaughtException was triggered via nextTick
process.on('beforeExit', common.mustCall(() => {
  assert.strictEqual(events.length, 4);
  assert.strictEqual(events[3], 'uncaughtException');
}));
