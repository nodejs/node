'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

setImmediate(() => {
  const store = { foo: 'bar' };
  asyncLocalStorage.enterWith(store);

  assert.strictEqual(asyncLocalStorage.getStore(), store);
  setTimeout(() => {
    assert.strictEqual(asyncLocalStorage.getStore(), store);
  }, 10);
});

setTimeout(() => {
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}, 10);
