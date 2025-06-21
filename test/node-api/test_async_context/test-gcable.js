'use strict';
// Flags: --gc-interval=100 --gc-global

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { createAsyncResource } = require(`./build/${common.buildType}/binding`);

// Test for https://github.com/nodejs/node/issues/27218:
// napi_async_destroy() can be called during a regular garbage collection run.

const hook_result = {
  id: null,
  init_called: false,
  destroy_called: false,
};

const test_hook = async_hooks.createHook({
  init: (id, type) => {
    if (type === 'test_async') {
      hook_result.id = id;
      hook_result.init_called = true;
    }
  },
  destroy: (id) => {
    if (id === hook_result.id) hook_result.destroy_called = true;
  },
});

test_hook.enable();
createAsyncResource({});

// Trigger GC. This does *not* use global.gc(), because what we want to verify
// is that `napi_async_destroy()` can be called when there is no JS context
// on the stack at the time of GC.
// Currently, using --gc-interval=100 + 1M elements seems to work fine for this.
const arr = new Array(1024 * 1024);
for (let i = 0; i < arr.length; i++)
  arr[i] = {};

assert.strictEqual(hook_result.destroy_called, false);
setImmediate(() => {
  assert.strictEqual(hook_result.destroy_called, true);
});
