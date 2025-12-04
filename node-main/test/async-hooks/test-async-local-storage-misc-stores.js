'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run('hello node', common.mustCall(() => {
  assert.strictEqual(asyncLocalStorage.getStore(), 'hello node');
}));

const runStore = { hello: 'node' };
asyncLocalStorage.run(runStore, common.mustCall(() => {
  assert.strictEqual(asyncLocalStorage.getStore(), runStore);
}));
