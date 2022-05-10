'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncResource, executionAsyncId } = require('async_hooks');

const thisArg = {};

const asyncId = executionAsyncId();
const fn1 = AsyncResource.bindCurrent(common.mustCall(() => {
  assert.strictEqual(executionAsyncId(), asyncId);
}, 2));
const fn2 = AsyncResource.bindCurrent(common.mustCall(function () {
  assert.strictEqual(executionAsyncId(), asyncId);
  assert.strictEqual(this, thisArg);
}, 2), thisArg);

fn1();
fn2();
AsyncResource.bind(() => {
  assert.notStrictEqual(executionAsyncId(), asyncId);
  fn1();
  fn2();
})();
