'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run(42, () => {
  assert.strictEqual(asyncLocalStorage.getStore(), 42);
});

const runStore = { foo: 'bar' };
asyncLocalStorage.run(runStore, () => {
  assert.strictEqual(asyncLocalStorage.getStore(), runStore);
});

asyncLocalStorage.runSyncAndReturn('hello node', () => {
  assert.strictEqual(asyncLocalStorage.getStore(), 'hello node');
});

const runSyncStore = { hello: 'node' };
asyncLocalStorage.runSyncAndReturn(runSyncStore, () => {
  assert.strictEqual(asyncLocalStorage.getStore(), runSyncStore);
});
