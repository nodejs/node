'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run({}, (runArg) => {
  assert.strictEqual(runArg, 1);
  asyncLocalStorage.exit((exitArg) => {
    assert.strictEqual(exitArg, 2);
  }, 2);
}, 1);

asyncLocalStorage.runSyncAndReturn({}, (runArg) => {
  assert.strictEqual(runArg, 'foo');
  asyncLocalStorage.exitSyncAndReturn((exitArg) => {
    assert.strictEqual(exitArg, 'bar');
  }, 'bar');
}, 'foo');
