'use strict';

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const binding = require(`./build/${common.buildType}/binding`);
const makeCallback = binding.makeCallback;

// Check async hooks integration using async context.
const hook_result = {
  id: null,
  init_called: false,
  before_called: false,
  after_called: false,
  destroy_called: false,
};
const test_hook = async_hooks.createHook({
  init: (id, type) => {
    if (type === 'test') {
      hook_result.id = id;
      hook_result.init_called = true;
    }
  },
  before: (id) => {
    if (id === hook_result.id) hook_result.before_called = true;
  },
  after: (id) => {
    if (id === hook_result.id) hook_result.after_called = true;
  },
  destroy: (id) => {
    if (id === hook_result.id) hook_result.destroy_called = true;
  },
});

test_hook.enable();
makeCallback(process, function() {});

assert.strictEqual(hook_result.init_called, true);
assert.strictEqual(hook_result.before_called, true);
assert.strictEqual(hook_result.after_called, true);
setImmediate(() => {
  assert.strictEqual(hook_result.destroy_called, true);
  test_hook.disable();
});
