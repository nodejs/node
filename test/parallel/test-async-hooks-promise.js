'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const initCalls = [];

async_hooks.createHook({
  init: common.mustCall((id, type, triggerId, resource) => {
    assert.strictEqual(type, 'PROMISE');
    initCalls.push({id, triggerId, resource});
  }, 2)
}).enable();

const a = Promise.resolve(42);
const b = a.then(common.mustCall());

assert.strictEqual(initCalls[0].triggerId, 1);
assert.strictEqual(initCalls[0].resource.parent, undefined);
assert.strictEqual(initCalls[0].resource.promise, a);
assert.strictEqual(initCalls[0].resource.getAsyncId(), initCalls[0].id);
assert.strictEqual(initCalls[1].triggerId, initCalls[0].id);
assert.strictEqual(initCalls[1].resource.parent, initCalls[0].resource);
assert.strictEqual(initCalls[1].resource.promise, b);
assert.strictEqual(initCalls[1].resource.getAsyncId(), initCalls[1].id);
