'use strict';

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const binding = require(`./build/${common.buildType}/binding`);

if (process.env.NODE_TEST_WITH_ASYNC_HOOKS) {
  common.skip('cannot test with env var NODE_TEST_WITH_ASYNC_HOOKS');
  return;
}

// Baseline to make sure the internal field isn't being set.
assert.strictEqual(
  binding.getPromiseField(Promise.resolve(1)),
  0);

const hook0 = async_hooks.createHook({}).enable();

// Check that no PromiseWrap is created when there are no hook callbacks.
assert.strictEqual(
  binding.getPromiseField(Promise.resolve(1)),
  0);

hook0.disable();

let pwrap = null;
const hook1 = async_hooks.createHook({
  init(id, type, tid, resource) {
    pwrap = resource;
  }
}).enable();

// Check that the internal field returns the same PromiseWrap passed to init().
assert.strictEqual(
  binding.getPromiseField(Promise.resolve(1)),
  pwrap);

hook1.disable();

// Check that internal fields are no longer being set. This needs to be delayed
// a bit because the `disable()` call only schedules disabling the hook in a
// future microtask.
setImmediate(() => {
  assert.strictEqual(
    binding.getPromiseField(Promise.resolve(1)),
    0);
});
