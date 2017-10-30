'use strict';
require('../common');

const assert = require('assert');
const async_hooks = require('async_hooks');
const triggerAsyncId = async_hooks.triggerAsyncId;

const triggerAsyncId0 = triggerAsyncId();
let triggerAsyncId1;

process.nextTick(() => {
  process.nextTick(() => {
    triggerAsyncId1 = triggerAsyncId();
    assert.notStrictEqual(
      triggerAsyncId0,
      triggerAsyncId1,
      'Async resources having different causal ancestry ' +
      'should have different triggerAsyncIds');
  });
  process.nextTick(() => {
    const triggerAsyncId2 = triggerAsyncId();
    assert.strictEqual(
      triggerAsyncId1,
      triggerAsyncId2,
      'Async resources having the same causal ancestry ' +
      'should have the same triggerAsyncId');
  });
});
