'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
let callbackToken = {};
let awaitToken = {};

let i = 0;
const exceptionHandler = common.mustCall(
  (err) => {
    ++i;
  assert.strictEqual(err.message, 'err2');
  assert.strictEqual(asyncLocalStorage.getStore(), callbackToken);
  }, 1);
process.setUncaughtExceptionCaptureCallback(exceptionHandler);

const rejectionHandler = common.mustCall((err) => {
  assert.strictEqual(err.message, 'err3');
  assert.strictEqual(asyncLocalStorage.getStore(), awaitToken);
}, 1);
process.on('unhandledRejection', rejectionHandler);

async function awaitTest() {
  await null;
  throw new Error('err3');
}
asyncLocalStorage.run(awaitToken, awaitTest);

try {
  asyncLocalStorage.run(callbackToken, () => {
    setTimeout(() => {
      process.nextTick(() => {
        assert.strictEqual(i, 1);
      });
      throw new Error('err2');
    }, 0);
    throw new Error('err1');
  });
} catch (e) {
  assert.strictEqual(e.message, 'err1');
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}
