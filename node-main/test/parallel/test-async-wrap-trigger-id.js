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
    // Async resources having different causal ancestry
    // should have different triggerAsyncIds
    assert.notStrictEqual(
      triggerAsyncId0,
      triggerAsyncId1);
  });
  process.nextTick(() => {
    const triggerAsyncId2 = triggerAsyncId();
    // Async resources having the same causal ancestry
    // should have the same triggerAsyncId
    assert.strictEqual(
      triggerAsyncId1,
      triggerAsyncId2);
  });
});
