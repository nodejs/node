'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const async_hooks = require('async_hooks');

assert.strictEqual(
  binding.getExecutionAsyncId(),
  async_hooks.executionAsyncId(),
);
assert.strictEqual(
  binding.getExecutionAsyncIdWithContext(),
  async_hooks.executionAsyncId(),
);
assert.strictEqual(
  binding.getTriggerAsyncId(),
  async_hooks.triggerAsyncId(),
);

process.nextTick(common.mustCall(() => {
  assert.strictEqual(
    binding.getExecutionAsyncId(),
    async_hooks.executionAsyncId(),
  );
  assert.strictEqual(
    binding.getExecutionAsyncIdWithContext(),
    async_hooks.executionAsyncId(),
  );
  assert.strictEqual(
    binding.getTriggerAsyncId(),
    async_hooks.triggerAsyncId(),
  );
}));
