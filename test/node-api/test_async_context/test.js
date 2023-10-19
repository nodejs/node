'use strict';
// Flags: --gc-interval=100 --gc-global

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const {
  makeCallback,
  createAsyncResource,
  destroyAsyncResource,
} = require(`./build/${common.buildType}/binding`);

const hook_result = {
  id: null,
  resource: null,
  init_called: false,
  destroy_called: false,
};

const test_hook = async_hooks.createHook({
  init: (id, type, triggerAsyncId, resource) => {
    if (type === 'test_async') {
      hook_result.id = id;
      hook_result.init_called = true;
      hook_result.resource = resource;
    }
  },
  destroy: (id) => {
    if (id === hook_result.id) hook_result.destroy_called = true;
  },
});

test_hook.enable();
const resourceWrap = createAsyncResource(
  /**
   * set resource to NULL to generate a managed resource object
   */
  undefined,
);

assert.strictEqual(hook_result.destroy_called, false);
const recv = {};
makeCallback(resourceWrap, recv, function callback() {
  assert.strictEqual(hook_result.destroy_called, false);
  assert.strictEqual(
    hook_result.resource,
    async_hooks.executionAsyncResource(),
  );
  assert.strictEqual(this, recv);

  setImmediate(() => {
    assert.strictEqual(hook_result.destroy_called, false);
    assert.notStrictEqual(
      hook_result.resource,
      async_hooks.executionAsyncResource(),
    );

    destroyAsyncResource(resourceWrap);
    setImmediate(() => {
      assert.strictEqual(hook_result.destroy_called, true);
    });
  });
});
