'use strict';
require('../common');
const assert = require('assert');
const {
  AsyncLocalStorage,
  executionAsyncResource
} = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

const outerResource = executionAsyncResource();

const store = new Map();
asyncLocalStorage.run(store, () => {
  assert.strictEqual(asyncLocalStorage.getStore(), store);
  const innerResource = executionAsyncResource();
  assert.notStrictEqual(innerResource, outerResource);
  asyncLocalStorage.run(store, () => {
    assert.strictEqual(asyncLocalStorage.getStore(), store);
    assert.strictEqual(executionAsyncResource(), innerResource);
    const otherStore = new Map();
    asyncLocalStorage.run(otherStore, () => {
      assert.strictEqual(asyncLocalStorage.getStore(), otherStore);
      assert.notStrictEqual(executionAsyncResource(), innerResource);
      assert.notStrictEqual(executionAsyncResource(), outerResource);
    });
  });
});

assert.strictEqual(executionAsyncResource(), outerResource);
