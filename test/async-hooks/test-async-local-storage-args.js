'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run({}, (runArg) => {
  assert.strictEqual(runArg, 'foo');
  asyncLocalStorage.exit((exitArg) => {
    assert.strictEqual(exitArg, 'bar');
  }, 'bar');
}, 'foo');
