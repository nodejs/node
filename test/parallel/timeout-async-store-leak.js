'use strict';

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const assert = require('assert');

const asyncLocalStorage = new AsyncLocalStorage();
asyncLocalStorage.run({}, common.mustCall(() => {
  const timeout = setTimeout(common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      assert.strictEqual(timeout[asyncLocalStorage.kResourceStore], undefined);
      assert.strictEqual(timeout._onTimeout, undefined);
    }))
  }));
}));
