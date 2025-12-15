'use strict';
// Test that trackPromises default to true.
const common = require('../common');
const { createHook } = require('node:async_hooks');
const assert = require('node:assert');

let res;
createHook({
  init: common.mustCall((asyncId, type, triggerAsyncId, resource) => {
    assert.strictEqual(type, 'PROMISE');
    res = resource;
  }),
}).enable();

const promise = Promise.resolve(1729);
assert.strictEqual(res, promise);
