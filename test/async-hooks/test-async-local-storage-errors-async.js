'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// case 1 fully async APIS (safe)
const asyncLocalStorage = new AsyncLocalStorage();

let i = 0;
process.setUncaughtExceptionCaptureCallback((err) => {
  ++i;
  assert.strictEqual(err.message, 'err' + i);
  assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'node');
});

asyncLocalStorage.run(new Map(), () => {
  const store = asyncLocalStorage.getStore();
  store.set('hello', 'node');
  setTimeout(() => {
    process.nextTick(() => {
      assert.strictEqual(i, 2);
    });
    throw new Error('err2');
  }, 0);
  throw new Error('err1');
});
