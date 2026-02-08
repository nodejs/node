/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test scope with thrown error
{
  const windowChannel = dc.windowChannel('test-scope-throw');
  const events = [];

  windowChannel.subscribe({
    start(message) {
      events.push({ type: 'start', data: message });
    },
    end(message) {
      events.push({ type: 'end', data: message });
    },
  });

  const context = { id: 1 };
  const testError = new Error('thrown error');

  assert.throws(() => {
    using scope = windowChannel.withScope(context);
    context.result = 'partial';
    throw testError;
  }, testError);

  // End event should still be published
  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[0].type, 'start');
  assert.strictEqual(events[1].type, 'end');

  // Context should have partial result but no error from throw
  assert.strictEqual(context.result, 'partial');
  assert.strictEqual(context.error, undefined);
}

// Test store restoration on error
{
  const windowChannel = dc.windowChannel('test-scope-store-error');
  const store = new AsyncLocalStorage();

  windowChannel.start.bindStore(store, (context) => context.value);

  windowChannel.subscribe({
    start() {},
    end() {},
  });

  store.enterWith('before');
  assert.strictEqual(store.getStore(), 'before');

  const testError = new Error('test');

  assert.throws(() => {
    using scope = windowChannel.withScope({ value: 'during' });
    assert.strictEqual(store.getStore(), 'during');
    throw testError;
  }, testError);

  // Store should be restored even after error
  assert.strictEqual(store.getStore(), 'before');
}

// Test dispose during exception handling
{
  const windowChannel = dc.windowChannel('test-scope-dispose-exception');
  const events = [];

  windowChannel.subscribe({
    start() {
      events.push('start');
    },
    end() {
      events.push('end');
    },
  });

  // Dispose should complete even when exception is thrown
  assert.throws(() => {
    using scope = windowChannel.withScope({});
    throw new Error('original error');
  }, /original error/);

  // End event should have been called
  assert.deepStrictEqual(events, ['start', 'end']);
}
