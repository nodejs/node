'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncResource, executionAsyncId } = require('async_hooks');

const fn = common.mustCall(AsyncResource.bind(() => {
  return executionAsyncId();
}));

setImmediate(() => {
  const asyncId = executionAsyncId();
  assert.notStrictEqual(asyncId, fn());
});

const asyncResource = new AsyncResource('test');

const fn2 = asyncResource.bind(() => {
  return executionAsyncId();
});

setImmediate(() => {
  const asyncId = executionAsyncId();
  assert.strictEqual(asyncResource.asyncId(), fn2());
  assert.notStrictEqual(asyncId, fn2());
});
