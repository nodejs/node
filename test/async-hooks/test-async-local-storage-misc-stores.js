'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run('hello node', () => {
  assert.strictEqual(asyncLocalStorage.getStore(), 'hello node');
});

const runStore = { hello: 'node' };
asyncLocalStorage.run(runStore, () => {
  assert.strictEqual(asyncLocalStorage.getStore(), runStore);
});
