'use strict';
require('../common');

const assert = require('assert');
const async_hooks = require('async_hooks');
const triggerId = async_hooks.triggerId;

const triggerId0 = triggerId();
let triggerId1;

process.nextTick(() => {
  process.nextTick(() => {
    triggerId1 = triggerId();
    assert.notStrictEqual(
      triggerId0,
      triggerId1,
      'Async resources having different causal ancestry ' +
      'should have different triggerIds');
  });
  process.nextTick(() => {
    const triggerId2 = triggerId();
    assert.strictEqual(
      triggerId1,
      triggerId2,
      'Async resources having the same causal ancestry ' +
      'should have the same triggerId');
  });
});
