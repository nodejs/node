/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test basic RunStoresScope with active channel
{
  const channel = dc.channel('test-run-stores-scope-basic');
  const store = new AsyncLocalStorage();
  const events = [];

  channel.subscribe((message) => {
    events.push({ type: 'message', data: message, store: store.getStore() });
  });

  channel.bindStore(store, (data) => {
    return { transformed: data.value };
  });

  assert.strictEqual(store.getStore(), undefined);

  {
    using scope = channel.withStoreScope({ value: 'test' });

    // Store should be set
    assert.deepStrictEqual(store.getStore(), { transformed: 'test' });

    events.push({ type: 'inside', store: store.getStore() });
  }

  // Store should be restored
  assert.strictEqual(store.getStore(), undefined);

  assert.strictEqual(events.length, 2);
  assert.strictEqual(events[0].type, 'message');
  assert.strictEqual(events[0].data.value, 'test');
  assert.deepStrictEqual(events[0].store, { transformed: 'test' });

  assert.strictEqual(events[1].type, 'inside');
  assert.deepStrictEqual(events[1].store, { transformed: 'test' });
}

// Test RunStoresScope with inactive channel (no-op)
{
  const channel = dc.channel('test-run-stores-scope-inactive');

  // No subscribers, channel is inactive
  {
    using scope = channel.withStoreScope({ value: 'test' });
    assert.ok(scope);
  }

  // Should not throw
}

// Test RunStoresScope restores previous store value
{
  const channel = dc.channel('test-run-stores-scope-restore');
  const store = new AsyncLocalStorage();

  channel.subscribe(() => {});
  channel.bindStore(store, (data) => data);

  store.enterWith('initial');
  assert.strictEqual(store.getStore(), 'initial');

  {
    using scope = channel.withStoreScope('scoped');
    assert.strictEqual(store.getStore(), 'scoped');
  }

  // Should restore to previous value
  assert.strictEqual(store.getStore(), 'initial');
}

// Test RunStoresScope with multiple stores
{
  const channel = dc.channel('test-run-stores-scope-multi');
  const store1 = new AsyncLocalStorage();
  const store2 = new AsyncLocalStorage();
  const store3 = new AsyncLocalStorage();

  channel.subscribe(() => {});
  channel.bindStore(store1, (data) => `${data}-1`);
  channel.bindStore(store2, (data) => `${data}-2`);
  channel.bindStore(store3, (data) => `${data}-3`);

  {
    using scope = channel.withStoreScope('test');

    assert.strictEqual(store1.getStore(), 'test-1');
    assert.strictEqual(store2.getStore(), 'test-2');
    assert.strictEqual(store3.getStore(), 'test-3');
  }

  assert.strictEqual(store1.getStore(), undefined);
  assert.strictEqual(store2.getStore(), undefined);
  assert.strictEqual(store3.getStore(), undefined);
}

// Test manual dispose via Symbol.dispose
{
  const channel = dc.channel('test-run-stores-scope-manual');
  const store = new AsyncLocalStorage();
  const events = [];

  channel.subscribe((message) => {
    events.push(message);
  });

  channel.bindStore(store, (data) => data);

  const scope = channel.withStoreScope('test');

  assert.strictEqual(events.length, 1);
  assert.strictEqual(store.getStore(), 'test');

  scope[Symbol.dispose]();

  // Store should be restored
  assert.strictEqual(store.getStore(), undefined);

  // Double dispose should be idempotent
  scope[Symbol.dispose]();
  assert.strictEqual(store.getStore(), undefined);
}

// Test nested RunStoresScope
{
  const channel = dc.channel('test-run-stores-scope-nested');
  const store = new AsyncLocalStorage();
  const storeValues = [];

  channel.subscribe(() => {});
  channel.bindStore(store, (data) => data);

  {
    using outer = channel.withStoreScope('outer');
    storeValues.push(store.getStore());

    {
      using inner = channel.withStoreScope('inner');
      storeValues.push(store.getStore());
    }

    // Should restore to outer
    storeValues.push(store.getStore());
  }

  // Should restore to undefined
  storeValues.push(store.getStore());

  assert.deepStrictEqual(storeValues, ['outer', 'inner', 'outer', undefined]);
}

// Test RunStoresScope with error during usage
{
  const channel = dc.channel('test-run-stores-scope-error');
  const store = new AsyncLocalStorage();

  channel.subscribe(() => {});
  channel.bindStore(store, (data) => data);

  store.enterWith('before');

  const testError = new Error('test');

  assert.throws(() => {
    using scope = channel.withStoreScope('during');
    assert.strictEqual(store.getStore(), 'during');
    throw testError;
  }, testError);

  // Store should be restored even after error
  assert.strictEqual(store.getStore(), 'before');
}

// Test RunStoresScope with inactive channel (no stores or subscribers)
{
  const channel = dc.channel('test-run-stores-scope-inactive');

  // Channel is inactive (no subscribers or bound stores)
  {
    using scope = channel.withStoreScope('test');
    // No-op disposable, nothing happens
    assert.ok(scope);
  }
}

// Test RunStoresScope with Symbol.dispose
{
  const channel = dc.channel('test-run-stores-scope-symbol');
  const store = new AsyncLocalStorage();

  channel.subscribe(() => {});
  channel.bindStore(store, (data) => data);

  const scope = channel.withStoreScope('test');
  assert.strictEqual(store.getStore(), 'test');

  // Call Symbol.dispose directly
  scope[Symbol.dispose]();
  assert.strictEqual(store.getStore(), undefined);
}
