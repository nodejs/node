'use strict';
const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test RunStoresScope with transform error
// Transform errors are scheduled via process.nextTick(triggerUncaughtException)

const channel = dc.channel('test-transform-error');
const store = new AsyncLocalStorage();
const events = [];

const transformError = new Error('transform failed');

// Set up uncaughtException handler to catch the transform error
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, transformError);
  events.push('uncaughtException');
}));

channel.subscribe((message) => {
  events.push({ type: 'message', data: message });
});

// Bind store with a transform that throws
channel.bindStore(store, () => {
  throw transformError;
});

// Store should remain undefined since transform failed
assert.strictEqual(store.getStore(), undefined);

{
  // eslint-disable-next-line no-unused-vars
  using scope = channel.withStoreScope({ value: 'test' });

  // Store should still be undefined because transform threw
  assert.strictEqual(store.getStore(), undefined);

  events.push('inside-scope');
}

// Store should still be undefined after scope exit
assert.strictEqual(store.getStore(), undefined);

// Message should still be published despite transform error
assert.strictEqual(events.length, 2);
assert.strictEqual(events[0].type, 'message');
assert.strictEqual(events[0].data.value, 'test');
assert.strictEqual(events[1], 'inside-scope');

// Validate uncaughtException was triggered via nextTick
process.on('beforeExit', common.mustCall(() => {
  assert.strictEqual(events.length, 3);
  assert.strictEqual(events[2], 'uncaughtException');
}));
