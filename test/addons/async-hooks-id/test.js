'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const async_hooks = require('async_hooks');

assert.strictEqual(
  binding.getExecutionAsyncId(),
  async_hooks.executionAsyncId()
);
assert.strictEqual(
  binding.getTriggerAsyncId(),
  async_hooks.triggerAsyncId()
);
assert.strictEqual(
  typeof binding.emitAsyncInit({}), 'number'
);

process.nextTick(common.mustCall(function() {
  assert.strictEqual(
    binding.getExecutionAsyncId(),
    async_hooks.executionAsyncId()
  );
  assert.strictEqual(
    binding.getTriggerAsyncId(),
    async_hooks.triggerAsyncId()
  );
}));
