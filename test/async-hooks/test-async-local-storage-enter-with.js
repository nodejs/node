'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

setImmediate(common.mustCall(() => {
  const store = { foo: 'bar' };
  asyncLocalStorage.enterWith(store);

  assert.strictEqual(asyncLocalStorage.getStore(), store);
  setTimeout(common.mustCall(() => {
    assert.strictEqual(asyncLocalStorage.getStore(), store);
  }), 10);
}));

setTimeout(common.mustCall(() => {
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}), 10);
