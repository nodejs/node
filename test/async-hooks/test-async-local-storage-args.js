'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run({}, common.mustCall((runArg) => {
  assert.strictEqual(runArg, 'foo');
  asyncLocalStorage.exit(common.mustCall((exitArg) => {
    assert.strictEqual(exitArg, 'bar');
  }), 'bar');
}), 'foo');
